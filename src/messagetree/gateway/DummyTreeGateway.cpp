#include "zg/messagetree/gateway/DummyTreeGateway.h"

namespace zg {

static DummyTreeGateway _dummyTreeGateway;
ITreeGateway * GetDummyTreeGateway() {return &_dummyTreeGateway;}

};

