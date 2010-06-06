#include "msock_internal.h"

static void engine_user_constructor(struct base *base,
				    struct engine_proto *proto,
				    int user_max_processes)
{
	domain_new(base, proto, NULL, user_max_processes);
	return;
}

static void engine_user_destructor(void *ingress_callback_data)
{
	return;
}

static void engine_user_ingress_callback(void *ingress_callback_data)
{
	return;
}

static struct engine_proto engine_user = {
	.name = NULL,
	.constructor = engine_user_constructor,
	.destructor = engine_user_destructor,
	.ingress_callback = engine_user_ingress_callback
};


REGISTER_ENGINE(MSOCK_ENGINE_MASK_USER, &engine_user);
