#include "HoneydTypesJs.h"
#include "v8Helper.h"
#include "Logger.h"

#include <v8.h>
#include <string>
#include <node.h>

using namespace v8;
using namespace Nova;
using namespace std;

Handle<Object> HoneydNodeJs::WrapNode(Node* node)
{
	HandleScope scope;
	// Setup the template for the type if it hasn't been already
	if( nodeTemplate.IsEmpty() )
	{
		Handle<FunctionTemplate> protoTemplate = FunctionTemplate::New();
		protoTemplate->InstanceTemplate()->SetInternalFieldCount(1);
		nodeTemplate = Persistent<FunctionTemplate>::New(protoTemplate);

		// Javascript methods
		Local<Template> proto = nodeTemplate->PrototypeTemplate();
		proto->Set("GetInterface",  FunctionTemplate::New(InvokeMethod<string, Node, &Nova::Node::GetInterface>) );
		proto->Set("GetProfile",    FunctionTemplate::New(InvokeMethod<string, Node, &Nova::Node::GetProfile>) );
		proto->Set("GetPortSet",    FunctionTemplate::New(InvokeMethod<string, Node, &Nova::Node::GetPortSet>) );
		proto->Set("GetIP",         FunctionTemplate::New(InvokeMethod<string, Node, &Nova::Node::GetIP>) );
		proto->Set("GetMAC",        FunctionTemplate::New(InvokeMethod<string, Node, &Nova::Node::GetMAC>) );
		proto->Set("IsEnabled",     FunctionTemplate::New(InvokeMethod<bool, Node, &Nova::Node::IsEnabled>) );
	}

	// Get the constructor from the template
	Handle<Function> ctor = nodeTemplate->GetFunction();
	// Instantiate the object with the constructor
	Handle<Object> result = ctor->NewInstance();
	// Wrap the native object in an handle and set it in the internal field to get at later.
	Handle<External> nodePtr = External::New(node);
	result->SetInternalField(0,nodePtr);

	return scope.Close(result);
}

Handle<Object> HoneydNodeJs::WrapPortSet(PortSet *portSet)
{
	HandleScope scope;

	// Setup the template for the type if it hasn't been already
	if( portSetTemplate.IsEmpty() )
	{
		Handle<FunctionTemplate> protoTemplate = FunctionTemplate::New();
		protoTemplate->InstanceTemplate()->SetInternalFieldCount(1);
		portSetTemplate = Persistent<FunctionTemplate>::New(protoTemplate);

		// Javascript methods
		Local<Template> proto = portSetTemplate->PrototypeTemplate();
		proto->Set("GetName",			FunctionTemplate::New(InvokeMethod<std::string, Nova::PortSet, &Nova::PortSet::GetName>) );
		proto->Set("GetTCPBehavior",	FunctionTemplate::New(InvokeMethod<std::string, Nova::PortSet, &Nova::PortSet::GetTCPBehavior>) );
		proto->Set("GetUDPBehavior",	FunctionTemplate::New(InvokeMethod<std::string, Nova::PortSet, &Nova::PortSet::GetUDPBehavior>) );
		proto->Set("GetICMPBehavior",	FunctionTemplate::New(InvokeMethod<std::string, Nova::PortSet, &Nova::PortSet::GetICMPBehavior>) );

		proto->Set(String::NewSymbol("GetTCPPorts"),FunctionTemplate::New(GetTCPPorts)->GetFunction());
		proto->Set(String::NewSymbol("GetUDPPorts"),FunctionTemplate::New(GetUDPPorts)->GetFunction());
	}

	// Get the constructor from the template
	Handle<Function> ctor = portSetTemplate->GetFunction();
	// Instantiate the object with the constructor
	Handle<Object> result = ctor->NewInstance();
	// Wrap the native object in an handle and set it in the internal field to get at later.
	Handle<External> portSetPtr = External::New(portSet);
	result->SetInternalField(0,portSetPtr);

	return scope.Close(result);
}


Handle<Object> HoneydNodeJs::WrapPort(Port *port)
{
    HandleScope scope;

    // Setup the template for the type if it hasn't been already
    if( portTemplate.IsEmpty() )
    {
        Handle<FunctionTemplate> protoTemplate = FunctionTemplate::New();
        protoTemplate->InstanceTemplate()->SetInternalFieldCount(1);
        portTemplate = Persistent<FunctionTemplate>::New(protoTemplate);

        // Javascript methods
        Local<Template> proto = portTemplate->PrototypeTemplate();
        proto->Set("GetPortNum",	FunctionTemplate::New(InvokeMethod<uint, Nova::Port, &Nova::Port::GetPortNum>) );
        proto->Set("GetProtocol",	FunctionTemplate::New(InvokeMethod<std::string, Nova::Port, &Nova::Port::GetProtocol>) );
        proto->Set("GetBehavior",	FunctionTemplate::New(InvokeMethod<std::string, Nova::Port, &Nova::Port::GetBehavior>) );
        proto->Set("GetScriptName",	FunctionTemplate::New(InvokeMethod<std::string, Nova::Port, &Nova::Port::GetScriptName>) );
        proto->Set("GetService",	FunctionTemplate::New(InvokeMethod<std::string, Nova::Port, &Nova::Port::GetService>) );
    }

    // Get the constructor from the template
    Handle<Function> ctor = portTemplate->GetFunction();
    // Instantiate the object with the constructor
    Handle<Object> result = ctor->NewInstance();
    // Wrap the native object in an handle and set it in the internal field to get at later.
    Handle<External> portPtr = External::New(port);
    result->SetInternalField(0,portPtr);

    return scope.Close(result);
}

Handle<Value> HoneydNodeJs::GetTCPPorts(const Arguments& args)
{
	HandleScope scope;

	v8::Local<v8::Array> portArray = v8::Array::New();

	PortSet *portSet = ObjectWrap::Unwrap<PortSet>(args.This());
	if(portSet == NULL)
	{
		return scope.Close(portArray);
	}

	for(uint i = 0; i < portSet->m_TCPexceptions.size(); i++)
	{
		Port *copy = new Port();
		*copy = portSet->m_TCPexceptions[i];
		portArray->Set(v8::Number::New(i), HoneydNodeJs::WrapPort(copy));
	}

	return scope.Close(portArray);
}

Handle<Value> HoneydNodeJs::GetUDPPorts(const Arguments& args)
{
	HandleScope scope;

	v8::Local<v8::Array> portArray = v8::Array::New();

	PortSet *portSet = ObjectWrap::Unwrap<PortSet>(args.This());
	if(portSet == NULL)
	{
		return scope.Close(portArray);
	}

	for(uint i = 0; i < portSet->m_UDPexceptions.size(); i++)
	{
		Port *copy = new Port();
		*copy = portSet->m_UDPexceptions[i];
		portArray->Set(v8::Number::New(i), HoneydNodeJs::WrapPort(copy));
	}

	return scope.Close(portArray);
}

Handle<Object> HoneydNodeJs::WrapProfile(Profile *pfile)
{
	HandleScope scope;
	// Setup the template for the type if it hasn't been already
	if( profileTemplate.IsEmpty() )
	{
		Handle<FunctionTemplate> protoTemplate = FunctionTemplate::New();
		protoTemplate->InstanceTemplate()->SetInternalFieldCount(1);
		profileTemplate = Persistent<FunctionTemplate>::New(protoTemplate);

		// Javascript methods
		Local<Template> proto = profileTemplate->PrototypeTemplate();
		proto->Set("GetName",			FunctionTemplate::New(InvokeMethod<std::string, Profile, &Nova::Profile::GetName>));
		proto->Set("GetPersonality",	FunctionTemplate::New(InvokeMethod<std::string, Profile, &Nova::Profile::GetPersonality>));
		proto->Set("GetUptimeMin",		FunctionTemplate::New(InvokeMethod<uint, Profile, &Nova::Profile::GetUptimeMin>));
		proto->Set("GetUptimeMax",		FunctionTemplate::New(InvokeMethod<uint, Profile, &Nova::Profile::GetUptimeMax>));
		proto->Set("GetDropRate",		FunctionTemplate::New(InvokeMethod<std::string, Profile, &Nova::Profile::GetDropRate>));
		proto->Set("GetIsGenerated",	FunctionTemplate::New(InvokeMethod<bool, Profile, &Nova::Profile::GetIsGenerated>));
		proto->Set("GetCount",			FunctionTemplate::New(InvokeMethod<uint32_t, Profile, &Nova::Profile::GetCount>));
		proto->Set("GetParentProfile",	FunctionTemplate::New(InvokeMethod<std::string, Profile, &Nova::Profile::GetParentProfile>));
		proto->Set("GetVendors",		FunctionTemplate::New(InvokeMethod<std::vector<std::string>, Profile, &Nova::Profile::GetVendors>));
		proto->Set("GetVendorCounts",	FunctionTemplate::New(InvokeMethod<std::vector<uint>, Profile, &Nova::Profile::GetVendorCounts>));

		proto->Set("IsPersonalityInherited",FunctionTemplate::New(InvokeMethod<bool, Profile, &Nova::Profile::IsPersonalityInherited>));
		proto->Set("IsUptimeInherited",     FunctionTemplate::New(InvokeMethod<bool, Profile, &Nova::Profile::IsUptimeInherited>));
		proto->Set("IsDropRateInherited",   FunctionTemplate::New(InvokeMethod<bool, Profile, &Nova::Profile::IsDropRateInherited>));
	}

	// Get the constructor from the template
	Handle<Function> ctor = profileTemplate->GetFunction();
	// Instantiate the object with the constructor
	Handle<Object> result = ctor->NewInstance();
	// Wrap the native object in an handle and set it in the internal field to get at later.
	Handle<External> profilePtr = External::New(pfile);
	result->SetInternalField(0,profilePtr);

	return scope.Close(result);
}

Persistent<FunctionTemplate> HoneydNodeJs::nodeTemplate;
Persistent<FunctionTemplate> HoneydNodeJs::portTemplate;
Persistent<FunctionTemplate> HoneydNodeJs::portSetTemplate;
Persistent<FunctionTemplate> HoneydNodeJs::profileTemplate;
