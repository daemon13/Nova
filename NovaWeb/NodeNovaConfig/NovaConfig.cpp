#define BUILDING_NODE_EXTENSION
#include <node.h>
#include "NovaConfigBinding.h"
#include "HoneydConfigBinding.h"
#include "VendorMacDbBinding.h"
#include "OsPersonalityDbBinding.h"

using namespace v8;

void InitAll(Handle<Object> target) {
  NovaConfigBinding::Init(target);
  HoneydConfigBinding::Init(target);
  VendorMacDbBinding::Init(target);
  OsPersonalityDbBinding::Init(target);
}

NODE_MODULE(novaconfig, InitAll)
