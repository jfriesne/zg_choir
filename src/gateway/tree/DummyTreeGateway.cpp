#include "zg/gateway/tree/DummyTreeGateway.h"

namespace zg {

static DummyTreeGateway _dummyTreeGateway;
ITreeGateway * GetDummyTreeGateway() {return &_dummyTreeGateway;}

};

