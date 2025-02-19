#include <extdll.h>
#include <meta_api.h>
#include <cbase.h>
#include <player.h>
#include <usercmd.h>

#include "sdk_util.h"
#include "../env.h"
#include "../node.h"
#include "../node_internals.h"
#include "modcore.h"

#if HAVE_OPENSSL
#include "../node_crypto.h"
#endif

#include "libplatform/libplatform.h"

using v8::Context;
using v8::Handle;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::SealHandleScope;
using v8::V8;

using node::Environment;

static bool abort_on_uncaught_exception = false;
static bool debug_wait_connect = false;
static bool trace_sync_io = false;

static Isolate* node_isolate = nullptr;
static Isolate* isolate = nullptr;
static v8::Platform* default_platform;

static node::NodeInstanceData* instance_data;
static Environment* env = nullptr;
int exit_code = 1;
int exec_argc;
const char** exec_argv;
static bool runnable = false;

static bool more;

Locker* locker;
Isolate::Scope* isolate_scope;
Local<Context> context;
Context::Scope* context_scope;

static void StartNodeInstanceMe() {
	do {
		v8::platform::PumpMessageLoop(default_platform, isolate);
		more = uv_run(env->event_loop(), UV_RUN_ONCE);

		if (more == false) {
			v8::platform::PumpMessageLoop(default_platform, isolate);
			node::EmitBeforeExit(env);

			more = uv_loop_alive(env->event_loop());
			if (uv_run(env->event_loop(), UV_RUN_NOWAIT) != 0)
			more = true;
		}
	} while (more == true);
}

void io_StartFrame (void) {
	if (runnable) {
		v8::platform::PumpMessageLoop(default_platform, isolate);
		uv_run(env->event_loop(), UV_RUN_NOWAIT);
	}

	SET_META_RESULT(MRES_IGNORED);
}

void io_GameInit (void) {
	UTIL_LogPrintf("io_GameInit\n");

	char *argv[] = {"node", "/root/dev/_test.js", NULL};
	int argc = sizeof(argv) / sizeof(char*) - 1;

	CHECK_GT(argc, 0);

	node::Init(&argc, const_cast<const char**>(argv), &exec_argc, &exec_argv);

	const int thread_pool_size = 4;
	default_platform = v8::platform::CreateDefaultPlatform(thread_pool_size);
	V8::InitializePlatform(default_platform);
	V8::Initialize();

	UTIL_LogPrintf("new node::NodeInstanceData\n");

	instance_data = new node::NodeInstanceData(node::NodeInstanceType::MAIN, uv_default_loop(), argc, const_cast<const char**>(argv), exec_argc, exec_argv, false);

	UTIL_LogPrintf("new node::NodeInstanceData ok.\n");

	isolate = Isolate::New();
	if (instance_data->is_main()) node_isolate = isolate;

	locker = new Locker(isolate);
	isolate_scope = new Isolate::Scope(isolate);
	HandleScope handle_scope(isolate);
	context = Context::New(isolate);
	env = node::CreateEnvironment(isolate, instance_data->event_loop(), context, instance_data->argc(), instance_data->argv(), instance_data->exec_argc(), instance_data->exec_argv());
	context_scope = new Context::Scope(context);

	UTIL_LogPrintf("context setup complete\n");

	if (instance_data->is_main()) env->set_using_abort_on_uncaught_exc(abort_on_uncaught_exception);

	node::LoadEnvironment(env);

	env->set_trace_sync_io(trace_sync_io);

	SealHandleScope seal(isolate);

	runnable = true;

	SET_META_RESULT(MRES_IGNORED);
}

void io_ServerDeactivate (void) {
	SET_META_RESULT(MRES_IGNORED);
}
