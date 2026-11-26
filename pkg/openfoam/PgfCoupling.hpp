/*************************************************************************
*  Copyright (C) 2026 by Tadeusz Rymarz                                  *
*                                                                        *
*  This program is free software; it is licensed under the terms of the  *
*  GNU General Public License v2 or later. See file LICENSE for details. *
*************************************************************************/
// Yade side of the porousGasificationFoam (PGF) coupling, protocol v2 (PGF #328).
//
// A thin adapter over PgfDemBridge::DemSide (extra/PgfDemBridge, built into this
// plugin by PgfDemBridgeCore.cpp): the core owns the handshake, message layout,
// neighbour graph, exchange and teardown; PGF's half is pgfDemCoupler.
//   launch    one mpirun starts both programs; ranks below NDEM run yade, the
//             others porousGasificationFoam -parallel -mpi-split-by-appnum
//             -mpi-threads. yade.mpy splits world (colour 3) at import.
//   exchange  every dataExchangeInterval DEM steps, on every DEM rank: each
//             owned coupled body's {pos, vel, lambda_} up, one value back:
//             lambdaDot [m/s] (lambdaMode 0) or the apparentVolume factor r
//             (lambdaMode 1); NaN if no OpenFOAM rank found the body.
//   end       OpenFOAM's last step stops the DEM run; close() (yade.mpy calls
//             it at exit) stops OpenFOAM if the DEM run ends first.
#ifdef YADE_MPI

#pragma once

#include <lib/base/Logging.hpp>
#include <lib/serialization/Serializable.hpp>
#include <core/Body.hpp>
#include <core/GlobalEngine.hpp>
#include <core/Omega.hpp>
#include <core/Scene.hpp>
#include <core/Subdomain.hpp>
#include <memory>
#include <string>
#include <vector>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wsuggest-override"
#pragma GCC diagnostic ignored "-Wcast-function-type"
#include <mpi.h>
#pragma GCC diagnostic pop
#include <extra/PgfDemBridge/src/PgfDemBridge.hpp>

namespace yade { // Cannot have #include directive inside.

class PgfCoupling : public GlobalEngine {
private:
	MPI_Comm* myComm_p = nullptr;
	int       localRank = 0, localCommSize = 1;
	bool      serialYade = true;
	bool      closed     = false;
	// Not owned by a smart pointer: an unclosed bridge must not be destroyed after
	// MPI_Finalize (its destructor aborts), so close() alone frees it.
	PgfDemBridge::DemSide* side = nullptr;
	std::vector<double>     up, values;
	std::vector<Body::id_t> sentIds;

	MPI_Comm selfComm() const { return myComm_p ? *myComm_p : MPI_COMM_SELF; }
	bool     isWorker() const { return serialYade or localRank > 0; }
	void     open();
	void     gatherBodies();
	void     apply();
	void     setExchangeInterval(Real yadeDt);

public:
	void setIdList(const std::vector<int>& ids) { bodyList = ids; }
	void insertBodyId(int id);
	bool eraseId(int id);
	std::vector<int> getIdList() const { return bodyList; }
	int  getNumBodies() const { return (int)bodyList.size(); }
	void setMyComm(boost::python::object py_comm)
	{
		if (import_mpi4py() < 0) return;
		myComm_p = PyMPIComm_Get(py_comm.ptr());
		if (myComm_p == NULL) LOG_ERROR("invalid COMM received from Python");
	}
	PyObject* getMyComm() { return PyMPIComm_New(selfComm()); }
	void      close();
	void      action() override;
	virtual ~PgfCoupling() {};

	std::vector<int> bodyList;

	YADE_CLASS_BASE_DOC_ATTRS_CTOR_PY(PgfCoupling, GlobalEngine,
	"Coupling with porousGasificationFoam (PGF #328, protocol v2, through PgfDemBridge). Sends each coupled body's position, velocity "
	"and :yref:`State.lambda_` to the OpenFOAM ranks whose mesh box holds its centre, and applies the value of the rank that found it: "
	"a rate set as :yref:`State.lambdaDot` (lambdaMode 0) or a factor multiplied into :yref:`State.lambda_` (lambdaMode 1). No "
	"hydrodynamic force or torque is applied. Start both programs with one mpirun and import yade.mpy; OpenFOAM's last step stops "
	"the DEM run, and a DEM run that ends first stops OpenFOAM. The startup handshake stops both sides by name on any mismatch.",
	((int, lambdaMode, -1, Attr::readonly, "Set by the handshake: 0 = rate, 1 = apparentVolume factor."))
	((int, expectLambdaMode, -1, , "If 0 or 1, the handshake stops unless OpenFOAM reports this lambdaMode; -1 accepts either."))
	((Real, foamDeltaT, 0, Attr::readonly, "OpenFOAM deltaT from the last exchange."))
	((long, dataExchangeInterval, 1, Attr::readonly, "DEM steps per exchange: foamDeltaT/O.dt (an exact integer in rate mode, #230)."))
	((int, nLost, 0, Attr::readonly, "Bodies sent at the last exchange that no OpenFOAM rank found (this rank)."))
	((bool, foamDone, false, Attr::readonly, "OpenFOAM ran its last step, or stopped; no further exchange happens."))
	,
	,
	.def("setIdList", &PgfCoupling::setIdList, boost::python::arg("bodyIdlist"), "Ids of the bodies in the coupling.")
	.def("insertBodyId", &PgfCoupling::insertBodyId, boost::python::arg("newId"), "Add a body to the coupling.")
	.def("eraseId", &PgfCoupling::eraseId, boost::python::arg("idToErase"), "Remove a body from the coupling.")
	.def("getIdList", &PgfCoupling::getIdList, "Ids of the bodies in the coupling.")
	.def("getNumBodies", &PgfCoupling::getNumBodies, "Number of bodies in the coupling.")
	.def("close", &PgfCoupling::close, "Collective on all ranks of both programs: end the coupling (stops OpenFOAM if it is still running). yade.mpy calls it at exit.")
	.add_property("comm", &PgfCoupling::getMyComm, &PgfCoupling::setMyComm, "Communicator to be used for MPI (converts mpi4py comm <-> c++ comm)")
	)
	// clang-format on
	DECLARE_LOGGER;
};
REGISTER_SERIALIZABLE(PgfCoupling);

} // namespace yade
#endif
