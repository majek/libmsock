#ifndef _MSOCK_ENGINE_H
#define _MSOCK_ENGINE_H

enum engine_types_internal {
	MSOCK_ENGINE_MASK_USER    = 1 << 1
};

struct domain;
struct base;

struct engine_proto {
	const char *name;
	void (*constructor)(struct base *, struct engine_proto *, int);
	void (*destructor)(void *ingress_callback_data);

	/* must be reentrant */
	void (*ingress_callback)(void *ingres_callback_data);
};



DLL_LOCAL void engines_start(struct base *base, int engine_mask, int max_processes);
DLL_LOCAL void engines_stop(struct base *base);

DLL_PUBLIC void msock_register_engine(int engine_type, struct engine_proto *proto);


/* gcc-specific hack - let's do the registration before main() */
#define REGISTER_ENGINE(type, proto)					\
	void __attribute ((constructor)) _msock_register_engine_##type() {	\
		msock_register_engine(type, proto);				\
	}

#endif // _MSOCK_ENGINE_H
