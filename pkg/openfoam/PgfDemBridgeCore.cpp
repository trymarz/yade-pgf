// Builds the PgfDemBridge core (extra/PgfDemBridge, GPL-3.0-or-later) into pkg_openfoam,
// which CMakeLists.txt globs from pkg/openfoam/*.cpp.
#ifdef YADE_MPI
#include <extra/PgfDemBridge/src/PgfDemBridge.cpp>
#endif
