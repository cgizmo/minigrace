#ifndef GRACELIB_H
#define GRACELIB_H

#include <stdint.h>
#include <setjmp.h>

#include "gracelib_types.h"
#include "gracelib_threads.h"

#define MFLAG_REALSELFONLY 2
#define MFLAG_REALSELFALSO 4
#define MFLAG_DEF 8
#define MFLAG_CONFIDENTIAL 16
#define MFLAG_PRIVATE 32

#define OFLAG_USEROBJ 16
#define OFLAG_MUTABLE 64

#define CFLAG_SELF 128

struct UserObject
{
    OBJECT_HEADER;
    jmp_buf *retpoint;
    Object super;
    Object *data;
    Object *annotations;
    int ndata;
    int numannotations;
};

struct OctetsObject
{
    OBJECT_HEADER;
    int blen;
    char body[];
};

Object alloc_Float64(double);
Object alloc_List();
Object alloc_BuiltinList();
Object alloc_String(const char *);
Object tailcall(Object, const char *, int, int *, Object *, int);
Object callmethod3(Object, const char *, int, int *, Object *, int);
Object callmethod(Object receiver, const char *name,
                  int nparts, int *nparams, Object *args);
Object callmethodflags(Object receiver, const char *name,
                       int nparts, int *nparams, Object *args, int callflags);
Object callmethod4(Object, const char *,
                   int, int *, Object *, int, int);
Object callmethodself(Object receiver, const char *name,
                      int nparts, int *nparamsv, Object *args);
Object alloc_Boolean(int val);
Object alloc_Octets(const char *data, int len);
Object alloc_ConcatString(Object, Object);
Object alloc_Undefined();
Object alloc_done();
Object alloc_ellipsis();
Object alloc_MatchFailed();
Object matchCase(Object, Object *, int, Object);
Object catchCase(Object, Object *, int, Object);
Object alloc_Integer32(int);
Object alloc_Block(Object self, Object(*body)(Object, int, Object *, int),
                   const char *, int);
Method *add_Method(ClassData, const char *,
                   Object(*func)(Object, int, int *, Object *, int));
struct MethodType *alloc_MethodType(int, int *);
Object alloc_obj(int, ClassData);
Object alloc_newobj(int, ClassData);
ClassData alloc_class(const char *, int);
ClassData alloc_class2(const char *, int, void(*)(void *));
ClassData alloc_class3(const char *, int, void(*)(void *), void(*)(void *));
Object alloc_Type(const char *, int);
Object alloc_userobj(int, int);
Object alloc_userobj2(int, int, ClassData);
Object alloc_userobj3(int, int, int, ClassData);
Object alloc_obj2(int, int);
Object *alloc_var();
Object alloc_HashMapClassObject();
Object gracelib_print(Object, int, Object *);
Object gracelib_length(Object);
void gracelib_raise_exception(char*);
Object dlmodule(const char *);
Object process_varargs(Object *, int, int);
void assertClass(Object, ClassData);

void bufferfromString(Object, char *);
char *cstringfromString(Object);
int integerfromAny(Object);
Object createclosure(int, char *);
struct StackFrameObject *alloc_StackFrame(int, struct StackFrameObject *);
void setclosureframe(Object, struct StackFrameObject *);
struct StackFrameObject *getclosureframe(Object);

void setline(int);
void gracedie(char *msg, ...);

void grace_register_shutdown_function(void(*)());
void grace_iterate(Object iterable, void(*callback)(Object, void *),
                   void *userdata);
Object alloc_SuccessfulMatch(Object result, Object bindings);
Object alloc_FailedMatch(Object result, Object bindings);

char *grcstring(Object);

// Prototypes used by dynamic-module objects
Object Object_Equals(Object, int, int *, Object *, int);
Object Object_NotEquals(Object, int, int *, Object *, int);
Object Object_asString(Object, int, int *, Object *, int);
Object Singleton_asString(Object, int, int *, Object *, int);

// Used for immutable obj copies in libs
Object immutable_primitive_copy(Object self, int nparams, int *argcv, Object *argv, int flags);

// These are used by code generation, and shouldn't need to be
// used elsewhere.
void initprofiling();
void gracelib_argv(char **);
void gracelib_destroy();
void module_sys_init_argv(Object);
void gracelib_stats();
void addtoclosure(Object, Object *);
void resolveclosure(Object);
void glfree(void *);
void setCompilerModulePath(char *);
void setModulePath(char *);
Object *getfromclosure(Object, int);
void addmethod2(Object, char *, Object (*)(Object, int, int *, Object *, int));
Method *addmethod2pos(Object, char *, Object (*)(Object, int, int *, Object *, int), int);
void addmethodreal(Object, char *, Object (*)(Object, int, int *, Object *, int));
Method *addmethodrealflags(Object, char *, Object (*)(Object, int, int *, Object *, int), int);
void adddatum2(Object, Object, int);
Object getdatum(Object, int, int);
Object gracebecome(Object, Object);
void set_type(Object, int16_t);
Object setsuperobj(Object, Object);
void block_savedest(Object);
void block_return(Object, Object);
void setclassname(Object, char *);
void pushstackframe(struct StackFrameObject *, char *name);
void pushclosure(Object);
void set_source_object(Object);
void setframeelementname(struct StackFrameObject *, int, char *);
void gracelib_stats();
int istrue(Object);
void setmodule(const char *);
void setsource(char *sl[]);
Object grace_userobj_outer(Object, int, int *, Object *, int);
Object grace_prelude();

// Used by GC
void debug(char *, ...);
void die(char *, ...);

#endif
