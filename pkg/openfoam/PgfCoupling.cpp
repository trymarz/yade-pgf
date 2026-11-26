/*************************************************************************
*  Copyright (C) 2026 by Tadeusz Rymarz                                  *
*                                                                        *
*  This program is free software; it is licensed under the terms of the  *
*  GNU General Public License v2 or later. See file LICENSE for details. *
*************************************************************************/
// Yade side of the porousGasificationFoam coupling; protocol in PgfCoupling.hpp.
#ifdef YADE_MPI

#include "PgfCoupling.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>

namespace yade { // Cannot have #include directive inside.

CREATE_LOGGER(PgfCoupling);
YADE_PLUGIN((PgfCoupling));

void PgfCoupling::insertBodyId(int id)
{
	if (std::find(bodyList.begin(), bodyList.end(), id) == bodyList.end()) bodyList.push_back(id);
}

bool PgfCoupling::eraseId(int id)
{
	auto it = std::find(bodyList.begin(), bodyList.end(), id);
	if (it == bodyList.end()) return false;
	bodyList.erase(it);
	return true;
}

void PgfCoupling::setFoamSolver(const std::string&, unsigned)
{
	LOG_WARN("PgfCoupling.SetOpenFoamSolver is ignored: protocol v2 starts OpenFOAM in the same mpirun (PGF #328)");
}

// Collective with OpenFOAM: every DEM rank, mpy's master included, joins the bridge.
void PgfCoupling::open()
{
	scene = Omega::instance().getScene().get();
	MPI_Comm_rank(selfComm(), &localRank);
	MPI_Comm_size(selfComm(), &localCommSize);
	serialYade = (localCommSize == 1);
	side       = new PgfDemBridge::DemSide(MPI_COMM_WORLD, expectLambdaMode);
	lambdaMode = int(side->lambdaMode());
	if (localRank == 0)
		std::cout << "PgfCoupling: handshake ok, protocol v" << PgfDemBridge::protocolVersion << ", lambdaMode " << lambdaMode << ", "
		          << side->nFoam() << " OpenFOAM rank(s)" << std::endl;
}

// Owned coupled bodies (all of them in serial Yade); erased ids are skipped.
void PgfCoupling::gatherBodies()
{
	up.clear();
	sentIds.clear();
	if (!isWorker()) return;
	for (const int id : bodyList) {
		if (id < 0 or !scene->bodies->exists(id)) continue;
		const shared_ptr<Body>& b = Body::byId(id, scene);
		if (!b or !b->state) continue;
		if (!serialYade and b->subdomain != scene->subdomain) continue;
		const Vector3r  pos = scene->isPeriodic ? scene->cell->wrapPt(b->state->pos) : b->state->pos;
		const Vector3r& v   = b->state->vel;
		sentIds.push_back(id);
		up.insert(up.end(), { double(pos[0]), double(pos[1]), double(pos[2]), double(v[0]), double(v[1]), double(v[2]), double(b->state->lambda_) });
	}
}

// One value per sent body, in order; NaN: no OpenFOAM rank found it.
void PgfCoupling::apply()
{
	for (std::size_t i = 0; i != sentIds.size(); ++i) {
		const double value = values[i];
		if (std::isnan(value)) continue;
		const shared_ptr<Body>& b = Body::byId(sentIds[i], scene);
		if (lambdaMode == 1) {
			if (value > 0) b->state->lambda_ *= value;
			b->state->lambdaDot = 0.0;
		} else {
			b->state->lambdaDot = value;
		}
	}
	nLost = side->nLost();
	if (nLost > 0) LOG_WARN("PgfCoupling: " << nLost << " coupled bodies were in no OpenFOAM rank's mesh at iter " << scene->iter);
}

// Rate mode integrates lambda every DEM step, so yadeDt must divide foamDt (#230).
void PgfCoupling::setExchangeInterval(Real yadeDt)
{
	if (lambdaMode == 1) {
		dataExchangeInterval = (long)((yadeDt < foamDeltaT) ? foamDeltaT / yadeDt : 1);
		return;
	}
	const Real ratio = foamDeltaT / yadeDt;
	const long n     = std::lround(ratio);
	if (n < 1 or std::abs(ratio - n) > 1e-6 * n) {
		std::ostringstream e;
		e << "in a lambdaDot rate mode Yade's dt must divide the OpenFOAM deltaT exactly (yadeDt <= foamDt, foamDt/yadeDt an "
		     "integer); got yadeDt = "
		  << yadeDt << ", foamDt = " << foamDeltaT << ", ratio = " << ratio << ". Set O.dt to foamDt/N.";
		std::cerr << "PgfCoupling: " << e.str() << std::endl;
		MPI_Abort(MPI_COMM_WORLD, 1);
	}
	dataExchangeInterval = n;
}

void PgfCoupling::close()
{
	if (closed or !side) return;
	side->close();
	delete side;
	side   = nullptr;
	closed = true;
}

void PgfCoupling::action()
{
	if (closed or foamDone) return;
	if (!side) open();
	if (scene->iter % dataExchangeInterval != 0) return;

	gatherBodies();
	values.assign(sentIds.size(), std::numeric_limits<double>::quiet_NaN());
	const PgfDemBridge::Control c = side->exchange(up.data(), sentIds.size(), values.data());
	if (!c.stopped) {
		apply();
		foamDeltaT  = c.dt;
		Real yadeDt = scene->dt;
		MPI_Allreduce(MPI_IN_PLACE, &yadeDt, 1, MPI_DOUBLE, MPI_MIN, selfComm());
		setExchangeInterval(yadeDt);
	}
	if (c.done) {
		// OpenFOAM's last step: finish its DEM interval, then end the run (at once if it stopped).
		foamDone          = true;
		scene->stopAtIter = scene->iter + (c.stopped ? 1 : dataExchangeInterval);
		close();
		if (localRank == 0)
			std::cout << "PgfCoupling: OpenFOAM " << (c.stopped ? "stopped" : "ran its last step") << "; the DEM run ends at iter "
			          << scene->stopAtIter << std::endl;
	}
}

} // namespace yade

#endif
