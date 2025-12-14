#ifndef __GMP_H__

#if defined (__cplusplus)
#include <iosfwd>   /* 用于std::istream, std::ostream, std::string */
#endif

#if ! defined (__GMP_WITHIN_CONFIGURE)
#  if defined( _MSC_VER )
#    if defined( _WIN64 )
#        define __GMP_BITS_PER_MP_LIMB    64
#        define BITS_PER_MP_LIMB          64
#        define GMP_LIMB_BITS             64
#        define SIZEOF_MP_LIMB_T           8
#        define _LONG_LONG_LIMB            1
#    elif defined( _WIN32 )
#        define __GMP_BITS_PER_MP_LIMB    32
#        define BITS_PER_MP_LIMB          32
#        define GMP_LIMB_BITS             32
#        define SIZEOF_MP_LIMB_T           4
#        ifdef _LONG_LONG_LIMB
#          undef _LONG_LONG_LIMB
#        endif
#    else
#        error 这是错误版本的gmp.h文件
#    endif
#  endif
#  define GMP_NAIL_BITS                    0
#endif
#define GMP_NUMB_BITS     (GMP_LIMB_BITS - GMP_NAIL_BITS)
#define GMP_NUMB_MASK     ((~ __GMP_CAST (mp_limb_t, 0)) >> GMP_NAIL_BITS)
#define GMP_NUMB_MAX      GMP_NUMB_MASK
#define GMP_NAIL_MASK     (~ GMP_NUMB_MASK)


/* 以下代码（在#ifndef __GNU_MP__下的所有内容）必须在gmp.h和mp.h中完全相同，
   以便在应用程序或库构建期间同时包含这两个文件。 */
#ifndef __GNU_MP__
#define __GNU_MP__ 4

#define __need_size_t  /* 告诉gcc stddef.h我们只需要size_t */
#if defined (__cplusplus)
#include <cstddef>     /* 用于size_t */
#else
#include <stddef.h>    /* 用于size_t */
#endif
#undef __need_size_t



/* __STDC__ - 一些ANSI编译器仅将其定义为0，因此使用"defined"而非"__STDC__-0"。
   特别是Sun workshop C 5.0将__STDC__设为0，但需要"##"进行令牌粘贴。

   _AIX - gnu ansidecl.h断言所有已知的AIX编译器都是ANSI标准，但
          并不总是定义__STDC__。

   __DECC - 当前版本的DEC C（例如5.9版本）是ANSI标准，但
            在默认模式下不定义__STDC__。不确定旧版本是否为K&R风格，
            但除非有人仍在使用旧版本，否则不必担心。

   _mips - gnu ansidecl.h表示RISC/OS MIPS编译器在SVR4模式下是ANSI标准，
           但不定义__STDC__。

   _MSC_VER - Microsoft C是ANSI标准，但除非使用/Za选项，否则__STDC__未定义
              （使用/Za选项时，它被定义为1）。

   _WIN32 - 由gnu ansidecl.h测试，大概假设所有w32编译器都是ansi标准。

   注意：这组相同的测试被gen-psqr.c和demos/expr/expr-impl.h使用，
         因此如果需要添加任何内容，请务必更新这些文件。 */

#if  defined (__STDC__)                                 \
|| defined (__cplusplus)                              \
    || defined (_AIX)                                     \
    || defined (__DECC)                                   \
    || (defined (__mips) && defined (_SYSTYPE_SVR4))      \
    || defined (_MSC_VER)                                 \
    || defined (_WIN32)
#define __GMP_HAVE_CONST        1
#define __GMP_HAVE_PROTOTYPES   1
#define __GMP_HAVE_TOKEN_PASTE  1
#else
#define __GMP_HAVE_CONST        0
#define __GMP_HAVE_PROTOTYPES   0
#define __GMP_HAVE_TOKEN_PASTE  0
#endif


#if __GMP_HAVE_CONST
#define __gmp_const   const
#define __gmp_signed  signed
#else
#define __gmp_const
#define __gmp_signed
#endif


/* __GMP_DECLSPEC支持Windows DLL版本的libgmp，在所有其他情况下为空。

   编译libgmp对象时，__GMP_DECLSPEC是导出指令，
   为应用程序编译时是导入指令。这两种情况通过GMP Makefiles定义的
   __GMP_WITHIN_GMP来区分（应用程序中不定义）。

   __GMP_DECLSPEC_XX类似地用于libgmpxx。__GMP_WITHIN_GMPXX
   表示正在构建libgmpxx，在这种情况下libgmpxx函数是导出，
   但可能被调用的libgmp函数是导入。

   libmp.la使用__GMP_DECLSPEC，就像它是libgmp.la一样。libgmp和
   libmp不相互调用，因此没有冲突或混淆。

   不使用Libtool的DLL_EXPORT定义。

   不支持同时构建静态和DLL版本的GMP。这样做意味着
   应用程序必须在链接时告诉我们两者中哪一个将被使用，
   如果手动使用GMP，这似乎非常繁琐且容易出错，
   并且从包的角度来看同样繁琐，因为autoconf和automake没有提供太多帮助。

   __GMP_DECLSPEC需要用于所有已文档化的全局函数和变量，
   gmp-impl.h等中的各种内部函数可以不加装饰。
   但测试程序或速度测量程序使用的内部函数应具有__GMP_DECLSPEC，
   当然常量或变量必须具有它，否则将解析错误的地址。

   在gcc中，__declspec可以放在函数原型的开头或结尾。

   在Microsoft C中，__declspec必须放在开头，或者在类型之后，
   如"void __declspec(...) *foo()"。没有__dllexport或任何东西来
   防止有人愚蠢地#define dllexport。_export曾经可用，但不再可用。

   在Borland C中，_export仍然存在，但需要在类型之后，
   如"void _export foo();"。可能需要更改__GMP_DECLSPEC语法以
   利用这一点。可能比它值得的麻烦更多。 */

#if defined (__GNUC__)
#define __GMP_DECLSPEC_EXPORT  __declspec(__dllexport__)
#define __GMP_DECLSPEC_IMPORT  __declspec(__dllimport__)
#endif
#if defined (_MSC_VER) || defined (__BORLANDC__)
#define __GMP_DECLSPEC_EXPORT  __declspec(dllexport)
#define __GMP_DECLSPEC_IMPORT  __declspec(dllimport)
#endif
#ifdef __WATCOMC__
#define __GMP_DECLSPEC_EXPORT  __export
#define __GMP_DECLSPEC_IMPORT  __import
#endif
#ifdef __IBMC__
#define __GMP_DECLSPEC_EXPORT  _Export
#define __GMP_DECLSPEC_IMPORT  _Import
#endif

#if __GMP_LIBGMP_DLL
#if __GMP_WITHIN_GMP
/* 编译为进入DLL libgmp */
#define __GMP_DECLSPEC  __GMP_DECLSPEC_EXPORT
#else
/* 编译为进入将链接到DLL libgmp的应用程序 */
#define __GMP_DECLSPEC  __GMP_DECLSPEC_IMPORT
#endif
#else
/* 所有其他情况 */
#define __GMP_DECLSPEC
#endif


#ifdef __GMP_SHORT_LIMB
    typedef unsigned int        mp_limb_t;
typedef int            mp_limb_signed_t;
#else
#ifdef _LONG_LONG_LIMB
typedef unsigned long long int    mp_limb_t;
typedef long long int        mp_limb_signed_t;
#else
typedef unsigned long int    mp_limb_t;
typedef long int        mp_limb_signed_t;
#endif
#endif

/* 注意，名称__mpz_struct会进入C++混淆的函数名称，
   这意味着虽然"__"暗示是内部的，但我们必须保留这个名称以保持二进制兼容性。 */
typedef struct
{
    int _mp_alloc;        /* 由_mp_d字段分配和指向的*limbs*数量 */
    int _mp_size;            /* abs(_mp_size)是最后一个字段指向的limbs数量。
                              如果_mp_size为负数，则表示负数。 */
    mp_limb_t *_mp_d;        /* 指向limbs的指针 */
} __mpz_struct;

#endif /* __GNU_MP__ */


typedef __mpz_struct MP_INT;    /* gmp 1源代码兼容性 */
typedef __mpz_struct mpz_t[1];

typedef mp_limb_t *        mp_ptr;
typedef __gmp_const mp_limb_t *    mp_srcptr;
#if defined (_CRAY) && ! defined (_CRAYMPP)
/* 普通`int`更快（48位） */
#define __GMP_MP_SIZE_T_INT     1
typedef int            mp_size_t;
typedef int            mp_exp_t;
#else
#define __GMP_MP_SIZE_T_INT     0
typedef long int        mp_size_t;
typedef long int        mp_exp_t;
#endif

typedef struct
{
    __mpz_struct _mp_num;
    __mpz_struct _mp_den;
} __mpq_struct;

typedef __mpq_struct MP_RAT;    /* gmp 1源代码兼容性 */
typedef __mpq_struct mpq_t[1];

typedef struct
{
    int _mp_prec;            /* 最大精度，以`mp_limb_t`的数量表示。
                              由mpf_init设置并由mpf_set_prec修改。
                              由_mp_d字段指向的区域包含`prec' + 1个limbs */
    int _mp_size;            /* abs(_mp_size)是最后一个字段指向的limbs数量。
                              如果_mp_size为负数，则表示负数 */
    mp_exp_t _mp_exp;        /* 指数，以`mp_limb_t`为基数 */
    mp_limb_t *_mp_d;        /* 指向limbs的指针 */
} __mpf_struct;

/* typedef __mpf_struct MP_FLOAT; */
typedef __mpf_struct mpf_t[1];

/* 可用的随机数生成算法 */
typedef enum
{
    GMP_RAND_ALG_DEFAULT = 0,
    GMP_RAND_ALG_LC = GMP_RAND_ALG_DEFAULT /* 线性同余 */
} gmp_randalg_t;

/* 随机状态结构 */
typedef struct
{
    mpz_t _mp_seed;      /* _mp_d成员指向生成器的状态 */
    gmp_randalg_t _mp_alg;  /* 当前未使用 */
    union {
        void *_mp_lc;         /* 指向函数指针结构的指针 */
    } _mp_algdata;
} __gmp_randstate_struct;
typedef __gmp_randstate_struct gmp_randstate_t[1];

/* gmp文件中函数声明的类型 */
/* ??? 不应该用这些污染用户命名空间 ??? */
typedef __gmp_const __mpz_struct *mpz_srcptr;
typedef __mpz_struct *mpz_ptr;
typedef __gmp_const __mpf_struct *mpf_srcptr;
typedef __mpf_struct *mpf_ptr;
typedef __gmp_const __mpq_struct *mpq_srcptr;
typedef __mpq_struct *mpq_ptr;


/* mp.h中不需要这个，所以放在__GNU_MP__公共部分之外 */
#if __GMP_LIBGMP_DLL
#if __GMP_WITHIN_GMPXX
/* 编译为进入DLL libgmpxx */
#define __GMP_DECLSPEC_XX  __GMP_DECLSPEC_EXPORT
#else
/* 编译为进入将链接到DLL libgmpxx的应用程序 */
#define __GMP_DECLSPEC_XX  __GMP_DECLSPEC_IMPORT
#endif
#else
/* 所有其他情况 */
#define __GMP_DECLSPEC_XX
#endif


#if __GMP_HAVE_PROTOTYPES
#define __GMP_PROTO(x) x
#else
#define __GMP_PROTO(x) ()
#endif

#ifndef __MPN
#if __GMP_HAVE_TOKEN_PASTE
#define __MPN(x) __gmpn_##x
#else
#define __MPN(x) __gmpn_/**/x
#endif
#endif

/* 作为参考，这里不能使用"defined(EOF)"。在g++ 2.95.4中，
   <iostream>定义EOF但不定义FILE。 */
#if defined (FILE)                                              \
|| defined (H_STDIO)                                          \
    || defined (_H_STDIO)               /* AIX */                 \
    || defined (_STDIO_H)               /* glibc, Sun, SCO */     \
    || defined (_STDIO_H_)              /* BSD, OSF */            \
    || defined (__STDIO_H)              /* Borland */             \
    || defined (__STDIO_H__)            /* IRIX */                \
    || defined (_STDIO_INCLUDED)        /* HPUX */                \
    || defined (__dj_include_stdio_h_)  /* DJGPP */               \
    || defined (_FILE_DEFINED)          /* Microsoft */           \
    || defined (__STDIO__)              /* Apple MPW MrC */       \
    || defined (_MSL_STDIO_H)           /* Metrowerks */          \
    || defined (_STDIO_H_INCLUDED)      /* QNX4 */        \
    || defined (_ISO_STDIO_ISO_H)       /* Sun C++ */
#define _GMP_H_HAVE_FILE 1
#endif

/* 在ISO C中，如果给出了涉及"struct obstack *"的原型而
   没有定义该结构，则该结构的作用域仅限于原型，
   如果随后真实定义它，则会导致冲突。因此，只有在有obstack.h时才给出原型 */
#if defined (_OBSTACK_H)   /* glibc <obstack.h> */
#define _GMP_H_HAVE_OBSTACK 1
#endif

/* 仅当通过应用程序包含<stdarg.h>或<varargs.h>而
   使得va_list可用时，才提供gmp_vprintf等的原型。
   通常va_list是一个typedef，因此不能直接测试，但C99
   指定va_start是一个宏（在过去系统上通常也是宏），因此查找它。

   <stdio.h>将为vprintf和vfprintf定义某种va_list，但
   我们不打算使用它，因为它不是标准的，并且
   使用gmp_vprintf等的应用程序几乎肯定需要
   整个<stdarg.h>或<varargs.h>。 */

#ifdef va_start
#define _GMP_H_HAVE_VA_LIST 1
#endif

/* 测试gcc版本是否>=主版本.次版本，按照glibc中的__GNUC_PREREQ */
#if defined (__GNUC__) && defined (__GNUC_MINOR__)
#define __GMP_GNUC_PREREQ(maj, min) \
           ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
#define __GMP_GNUC_PREREQ(maj, min)  0
#endif

/* "pure"在gcc 2.96及更高版本中可用，参见"(gcc)Function Attributes"。
   基本上它意味着函数除了检查其参数和内存（全局或通过参数）以
   生成返回值外，不执行任何操作，但不改变任何内容且没有副作用。
   __GMP_NO_ATTRIBUTE_CONST_PURE允许tune/common.c等
   在尝试编写计时循环时关闭此功能。 */
#if __GMP_GNUC_PREREQ (2,96) && ! defined (__GMP_NO_ATTRIBUTE_CONST_PURE)
#define __GMP_ATTRIBUTE_PURE   __attribute__ ((__pure__))
#else
#define __GMP_ATTRIBUTE_PURE
#endif


/* __GMP_CAST允许我们在C++中使用static_cast，因此我们的宏对
   "g++ -Wold-style-cast"是干净的。

   extern "C"块内"extern inline"代码中的转换不会引起
   这些警告，因此__GMP_CAST只需要在已文档化的宏上使用 */

#ifdef __cplusplus
#define __GMP_CAST(type, expr)  (static_cast<type> (expr))
#else
#define __GMP_CAST(type, expr)  ((type) (expr))
#endif


/* 空的"throw ()"意味着函数不抛出任何C++异常，
   这可以在应用程序中节省一些堆栈帧信息。

   目前仅在不除以零等、不分配内存且预期永远不需要
   分配内存的函数上给出。这为将来GMP异常方案中的
   C++ throw留下了可能性。

   mpz_set_ui等被省略以为doc/tasks.html中描述的
   延迟分配方案留出空间。mpz_get_d等被省略以为
   浮点溢出异常留出空间。

   请注意，__GMP_NOTHROW必须在任何内联函数上给出，
   与它们的原型相同（至少对于g++，它们一起使用）。
   还请注意，g++ 3.0要求__GMP_NOTHROW在__GMP_ATTRIBUTE_PURE等其他属性之前 */

#if defined (__cplusplus)
#define __GMP_NOTHROW  throw ()
#else
#define __GMP_NOTHROW
#endif


/* PORTME: 其他什么编译器有有用的"extern inline"？如果编译器（或链接器）
   丢弃未使用的静态变量，"static inline"将是可接受的替代品 */

/* gcc在所有模式下都有__inline__，包括严格ansi。也为内联函数提供原型，
    以便在Windows上正确指定"dllimport"，以防函数被调用而不是内联。
    GCC 4.3及更高版本在使用-std=c99或-std=gnu99时实现ISO C99
    内联语义，除非使用-fgnu89-inline */
#ifdef __GNUC__
#ifdef __GNUC_STDC_INLINE__
#define __GMP_EXTERN_INLINE extern __inline__ __attribute__ ((__gnu_inline__))
#else
#define __GMP_EXTERN_INLINE      extern __inline__
#endif
#define __GMP_INLINE_PROTOTYPES  1
#endif

/* DEC C（例如5.9版本）支持"static __inline foo()"，即使在-std1严格ANSI模式下。
   即使不优化（即-O0模式，这是默认模式），也会进行内联，
   但会发出不必要的foo本地副本，除非使用-O。"extern __inline"被接受，
   但"extern"似乎被忽略，即它成为一个普通全局函数但在其文件内内联。
   不知道所有旧版本的DEC C是否支持__inline，但作为一个开始，
   让我们为当前版本做正确的事情。 */
#ifdef __DECC
#define __GMP_EXTERN_INLINE  static __inline
#endif

/* SCO OpenUNIX 8 cc支持"static inline foo()"但在-Xc严格
   ANSI模式下不支持（在该模式下__STDC__为1）。内联仅在-O下实际发生。
   没有-O时，"foo"似乎无论是否使用都会发出，这是浪费的。
   "extern inline foo()"没有用，"extern"显然被忽略，因此
   foo如果可能则内联，但也作为全局发出，这在构建共享libgmp时
   导致多重定义错误 */
#ifdef __SCO_VERSION__
#if __SCO_VERSION__ > 400000000 && __STDC__ != 1 \
           && ! defined (__GMP_EXTERN_INLINE)
#define __GMP_EXTERN_INLINE  static inline
#endif
#endif

/* C++总是有"inline"，由于它是一个正常功能，链接器应该丢弃
   重复的非内联副本，或者如果它不这样做，那么这对每个人都是问题，
   不仅仅是GMP */
#if defined (__cplusplus) && ! defined (__GMP_EXTERN_INLINE)
#define __GMP_EXTERN_INLINE  inline
#endif

/* 不要在配置运行期间进行任何内联，因为如果编译器最终将代码副本
   发射到目标文件中，它可能最终需要各种支持例程（如mpn_popcount）进行链接，
   导致"alloca"测试和可能的其他测试失败。在hppa ia64上，
   预发布的gcc 3.2被视为不尊重"extern __inline__"中的"extern"，
   也触发了这个问题 */
#if defined (__GMP_WITHIN_CONFIGURE) && ! __GMP_WITHIN_CONFIGURE_INLINE
#undef __GMP_EXTERN_INLINE
#endif

/* 默认情况下，当有内联版本时，不提供原型。特别要注意，
   Cray C++反对原型和内联的组合 */
#ifdef __GMP_EXTERN_INLINE
#ifndef __GMP_INLINE_PROTOTYPES
#define __GMP_INLINE_PROTOTYPES  0
#endif
#else
#define __GMP_INLINE_PROTOTYPES  1
#endif


#define __GMP_ABS(x)   ((x) >= 0 ? (x) : -(x))
#define __GMP_MAX(h,i) ((h) > (i) ? (h) : (i))

/* __GMP_USHRT_MAX不是"~ (unsigned short) 0"，因为short被"~"提升为int */
#define __GMP_UINT_MAX   (~ (unsigned) 0)
#define __GMP_ULONG_MAX  (~ (unsigned long) 0)
#define __GMP_USHRT_MAX  ((unsigned short) ~0)


/* __builtin_expect在gcc 3.0中，不在2.95中 */
#if __GMP_GNUC_PREREQ (3,0)
#define __GMP_LIKELY(cond)    __builtin_expect ((cond) != 0, 1)
#define __GMP_UNLIKELY(cond)  __builtin_expect ((cond) != 0, 0)
#else
#define __GMP_LIKELY(cond)    (cond)
#define __GMP_UNLIKELY(cond)  (cond)
#endif

#ifdef _CRAY
#define __GMP_CRAY_Pragma(str)  _Pragma (str)
#else
#define __GMP_CRAY_Pragma(str)
#endif


/* 允许直接用户访问mpq_t对象的分子和分母 */
#define mpq_numref(Q) (&((Q)->_mp_num))
#define mpq_denref(Q) (&((Q)->_mp_den))


#if defined (__cplusplus)
              extern "C" {
#ifdef _GMP_H_HAVE_FILE
#endif
#endif

#define mp_set_memory_functions __gmp_set_memory_functions
    __GMP_DECLSPEC void mp_set_memory_functions __GMP_PROTO ((void *(*) (size_t),
                                                             void *(*) (void *, size_t, size_t),
                                                             void (*) (void *, size_t))) __GMP_NOTHROW;

#define mp_get_memory_functions __gmp_get_memory_functions
    __GMP_DECLSPEC void mp_get_memory_functions __GMP_PROTO ((void *(**) (size_t),
                                                             void *(**) (void *, size_t, size_t),
                                                             void (**) (void *, size_t))) __GMP_NOTHROW;

#define mp_bits_per_limb __gmp_bits_per_limb
    __GMP_DECLSPEC extern __gmp_const int mp_bits_per_limb;

#define gmp_errno __gmp_errno
    __GMP_DECLSPEC extern int gmp_errno;

#define gmp_version __gmp_version
    __GMP_DECLSPEC extern __gmp_const char * __gmp_const gmp_version;


/**************** 随机数例程 ****************/

/* 已过时 */
#define gmp_randinit __gmp_randinit
    __GMP_DECLSPEC void gmp_randinit __GMP_PROTO ((gmp_randstate_t, gmp_randalg_t, ...));

#define gmp_randinit_default __gmp_randinit_default
    __GMP_DECLSPEC void gmp_randinit_default __GMP_PROTO ((gmp_randstate_t));

#define gmp_randinit_lc_2exp __gmp_randinit_lc_2exp
    __GMP_DECLSPEC void gmp_randinit_lc_2exp __GMP_PROTO ((gmp_randstate_t,
                                                          mpz_srcptr, unsigned long int,
                                                          unsigned long int));

#define gmp_randinit_lc_2exp_size __gmp_randinit_lc_2exp_size
    __GMP_DECLSPEC int gmp_randinit_lc_2exp_size __GMP_PROTO ((gmp_randstate_t, unsigned long));

#define gmp_randinit_mt __gmp_randinit_mt
    __GMP_DECLSPEC void gmp_randinit_mt __GMP_PROTO ((gmp_randstate_t));

#define gmp_randinit_set __gmp_randinit_set
    void gmp_randinit_set __GMP_PROTO ((gmp_randstate_t, __gmp_const __gmp_randstate_struct *));

#define gmp_randseed __gmp_randseed
    __GMP_DECLSPEC void gmp_randseed __GMP_PROTO ((gmp_randstate_t, mpz_srcptr));

#define gmp_randseed_ui __gmp_randseed_ui
    __GMP_DECLSPEC void gmp_randseed_ui __GMP_PROTO ((gmp_randstate_t, unsigned long int));

#define gmp_randclear __gmp_randclear
    __GMP_DECLSPEC void gmp_randclear __GMP_PROTO ((gmp_randstate_t));

#define gmp_urandomb_ui __gmp_urandomb_ui
    unsigned long gmp_urandomb_ui __GMP_PROTO ((gmp_randstate_t, unsigned long));

#define gmp_urandomm_ui __gmp_urandomm_ui
    unsigned long gmp_urandomm_ui __GMP_PROTO ((gmp_randstate_t, unsigned long));


/**************** 格式化输出例程 ****************/

#define gmp_asprintf __gmp_asprintf
    __GMP_DECLSPEC int gmp_asprintf __GMP_PROTO ((char **, __gmp_const char *, ...));

#define gmp_fprintf __gmp_fprintf
#ifdef _GMP_H_HAVE_FILE
    __GMP_DECLSPEC int gmp_fprintf __GMP_PROTO ((FILE *, __gmp_const char *, ...));
#endif

#define gmp_obstack_printf __gmp_obstack_printf
#if defined (_GMP_H_HAVE_OBSTACK)
    __GMP_DECLSPEC int gmp_obstack_printf __GMP_PROTO ((struct obstack *, __gmp_const char *, ...));
#endif

#define gmp_obstack_vprintf __gmp_obstack_vprintf
#if defined (_GMP_H_HAVE_OBSTACK) && defined (_GMP_H_HAVE_VA_LIST)
    __GMP_DECLSPEC int gmp_obstack_vprintf __GMP_PROTO ((struct obstack *, __gmp_const char *, va_list));
#endif

#define gmp_printf __gmp_printf
    __GMP_DECLSPEC int gmp_printf __GMP_PROTO ((__gmp_const char *, ...));

#define gmp_snprintf __gmp_snprintf
    __GMP_DECLSPEC int gmp_snprintf __GMP_PROTO ((char *, size_t, __gmp_const char *, ...));

#define gmp_sprintf __gmp_sprintf
    __GMP_DECLSPEC int gmp_sprintf __GMP_PROTO ((char *, __gmp_const char *, ...));

#define gmp_vasprintf __gmp_vasprintf
#if defined (_GMP_H_HAVE_VA_LIST)
    __GMP_DECLSPEC int gmp_vasprintf __GMP_PROTO ((char **, __gmp_const char *, va_list));
#endif

#define gmp_vfprintf __gmp_vfprintf
#if defined (_GMP_H_HAVE_FILE) && defined (_GMP_H_HAVE_VA_LIST)
    __GMP_DECLSPEC int gmp_vfprintf __GMP_PROTO ((FILE *, __gmp_const char *, va_list));
#endif

#define gmp_vprintf __gmp_vprintf
#if defined (_GMP_H_HAVE_VA_LIST)
    __GMP_DECLSPEC int gmp_vprintf __GMP_PROTO ((__gmp_const char *, va_list));
#endif

#define gmp_vsnprintf __gmp_vsnprintf
#if defined (_GMP_H_HAVE_VA_LIST)
    __GMP_DECLSPEC int gmp_vsnprintf __GMP_PROTO ((char *, size_t, __gmp_const char *, va_list));
#endif

#define gmp_vsprintf __gmp_vsprintf
#if defined (_GMP_H_HAVE_VA_LIST)
    __GMP_DECLSPEC int gmp_vsprintf __GMP_PROTO ((char *, __gmp_const char *, va_list));
#endif


/**************** 格式化输入例程 ****************/

#define gmp_fscanf __gmp_fscanf
#ifdef _GMP_H_HAVE_FILE
    __GMP_DECLSPEC int gmp_fscanf __GMP_PROTO ((FILE *, __gmp_const char *, ...));
#endif

#define gmp_scanf __gmp_scanf
    __GMP_DECLSPEC int gmp_scanf __GMP_PROTO ((__gmp_const char *, ...));

#define gmp_sscanf __gmp_sscanf
    __GMP_DECLSPEC int gmp_sscanf __GMP_PROTO ((__gmp_const char *, __gmp_const char *, ...));

#define gmp_vfscanf __gmp_vfscanf
#if defined (_GMP_H_HAVE_FILE) && defined (_GMP_H_HAVE_VA_LIST)
    __GMP_DECLSPEC int gmp_vfscanf __GMP_PROTO ((FILE *, __gmp_const char *, va_list));
#endif

#define gmp_vscanf __gmp_vscanf
#if defined (_GMP_H_HAVE_VA_LIST)
    __GMP_DECLSPEC int gmp_vscanf __GMP_PROTO ((__gmp_const char *, va_list));
#endif

#define gmp_vsscanf __gmp_vsscanf
#if defined (_GMP_H_HAVE_VA_LIST)
    __GMP_DECLSPEC int gmp_vsscanf __GMP_PROTO ((__gmp_const char *, __gmp_const char *, va_list));
#endif


/**************** 整数（即Z）例程 ****************/

#define _mpz_realloc __gmpz_realloc
#define mpz_realloc __gmpz_realloc
    __GMP_DECLSPEC void *_mpz_realloc __GMP_PROTO ((mpz_ptr, mp_size_t));

#define mpz_abs __gmpz_abs
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpz_abs)
    __GMP_DECLSPEC void mpz_abs __GMP_PROTO ((mpz_ptr, mpz_srcptr));
#endif

#define mpz_add __gmpz_add
    __GMP_DECLSPEC void mpz_add __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_add_ui __gmpz_add_ui
    __GMP_DECLSPEC void mpz_add_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_addmul __gmpz_addmul
    __GMP_DECLSPEC void mpz_addmul __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_addmul_ui __gmpz_addmul_ui
    __GMP_DECLSPEC void mpz_addmul_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_and __gmpz_and
    __GMP_DECLSPEC void mpz_and __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_array_init __gmpz_array_init
    __GMP_DECLSPEC void mpz_array_init __GMP_PROTO ((mpz_ptr, mp_size_t, mp_size_t));

#define mpz_bin_ui __gmpz_bin_ui
    __GMP_DECLSPEC void mpz_bin_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_bin_uiui __gmpz_bin_uiui
    __GMP_DECLSPEC void mpz_bin_uiui __GMP_PROTO ((mpz_ptr, unsigned long int, unsigned long int));

#define mpz_cdiv_q __gmpz_cdiv_q
    __GMP_DECLSPEC void mpz_cdiv_q __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_cdiv_q_2exp __gmpz_cdiv_q_2exp
    __GMP_DECLSPEC void mpz_cdiv_q_2exp __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long));

#define mpz_cdiv_q_ui __gmpz_cdiv_q_ui
    __GMP_DECLSPEC unsigned long int mpz_cdiv_q_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_cdiv_qr __gmpz_cdiv_qr
    __GMP_DECLSPEC void mpz_cdiv_qr __GMP_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_cdiv_qr_ui __gmpz_cdiv_qr_ui
    __GMP_DECLSPEC unsigned long int mpz_cdiv_qr_ui __GMP_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_cdiv_r __gmpz_cdiv_r
    __GMP_DECLSPEC void mpz_cdiv_r __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_cdiv_r_2exp __gmpz_cdiv_r_2exp
    __GMP_DECLSPEC void mpz_cdiv_r_2exp __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long));

#define mpz_cdiv_r_ui __gmpz_cdiv_r_ui
    __GMP_DECLSPEC unsigned long int mpz_cdiv_r_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_cdiv_ui __gmpz_cdiv_ui
    __GMP_DECLSPEC unsigned long int mpz_cdiv_ui __GMP_PROTO ((mpz_srcptr, unsigned long int)) __GMP_ATTRIBUTE_PURE;

#define mpz_clear __gmpz_clear
    __GMP_DECLSPEC void mpz_clear __GMP_PROTO ((mpz_ptr));

#define mpz_clrbit __gmpz_clrbit
    __GMP_DECLSPEC void mpz_clrbit __GMP_PROTO ((mpz_ptr, unsigned long int));

#define mpz_cmp __gmpz_cmp
    __GMP_DECLSPEC int mpz_cmp __GMP_PROTO ((mpz_srcptr, mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_cmp_d __gmpz_cmp_d
    __GMP_DECLSPEC int mpz_cmp_d __GMP_PROTO ((mpz_srcptr, double)) __GMP_ATTRIBUTE_PURE;

#define _mpz_cmp_si __gmpz_cmp_si
    __GMP_DECLSPEC int _mpz_cmp_si __GMP_PROTO ((mpz_srcptr, signed long int)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define _mpz_cmp_ui __gmpz_cmp_ui
    __GMP_DECLSPEC int _mpz_cmp_ui __GMP_PROTO ((mpz_srcptr, unsigned long int)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_cmpabs __gmpz_cmpabs
    __GMP_DECLSPEC int mpz_cmpabs __GMP_PROTO ((mpz_srcptr, mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_cmpabs_d __gmpz_cmpabs_d
    __GMP_DECLSPEC int mpz_cmpabs_d __GMP_PROTO ((mpz_srcptr, double)) __GMP_ATTRIBUTE_PURE;

#define mpz_cmpabs_ui __gmpz_cmpabs_ui
    __GMP_DECLSPEC int mpz_cmpabs_ui __GMP_PROTO ((mpz_srcptr, unsigned long int)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_com __gmpz_com
    __GMP_DECLSPEC void mpz_com __GMP_PROTO ((mpz_ptr, mpz_srcptr));

#define mpz_combit __gmpz_combit
    __GMP_DECLSPEC void mpz_combit __GMP_PROTO ((mpz_ptr, unsigned long int));

#define mpz_congruent_p __gmpz_congruent_p
    __GMP_DECLSPEC int mpz_congruent_p __GMP_PROTO ((mpz_srcptr, mpz_srcptr, mpz_srcptr)) __GMP_ATTRIBUTE_PURE;

#define mpz_congruent_2exp_p __gmpz_congruent_2exp_p
    __GMP_DECLSPEC int mpz_congruent_2exp_p __GMP_PROTO ((mpz_srcptr, mpz_srcptr, unsigned long)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_congruent_ui_p __gmpz_congruent_ui_p
    __GMP_DECLSPEC int mpz_congruent_ui_p __GMP_PROTO ((mpz_srcptr, unsigned long, unsigned long)) __GMP_ATTRIBUTE_PURE;

#define mpz_divexact __gmpz_divexact
    __GMP_DECLSPEC void mpz_divexact __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_divexact_ui __gmpz_divexact_ui
    __GMP_DECLSPEC void mpz_divexact_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long));

#define mpz_divisible_p __gmpz_divisible_p
    __GMP_DECLSPEC int mpz_divisible_p __GMP_PROTO ((mpz_srcptr, mpz_srcptr)) __GMP_ATTRIBUTE_PURE;

#define mpz_divisible_ui_p __gmpz_divisible_ui_p
    __GMP_DECLSPEC int mpz_divisible_ui_p __GMP_PROTO ((mpz_srcptr, unsigned long)) __GMP_ATTRIBUTE_PURE;

#define mpz_divisible_2exp_p __gmpz_divisible_2exp_p
    __GMP_DECLSPEC int mpz_divisible_2exp_p __GMP_PROTO ((mpz_srcptr, unsigned long)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_dump __gmpz_dump
    __GMP_DECLSPEC void mpz_dump __GMP_PROTO ((mpz_srcptr));

#define mpz_export __gmpz_export
    __GMP_DECLSPEC void *mpz_export __GMP_PROTO ((void *, size_t *, int, size_t, int, size_t, mpz_srcptr));

#define mpz_fac_ui __gmpz_fac_ui
    __GMP_DECLSPEC void mpz_fac_ui __GMP_PROTO ((mpz_ptr, unsigned long int));

#define mpz_fdiv_q __gmpz_fdiv_q
    __GMP_DECLSPEC void mpz_fdiv_q __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_fdiv_q_2exp __gmpz_fdiv_q_2exp
    __GMP_DECLSPEC void mpz_fdiv_q_2exp __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_fdiv_q_ui __gmpz_fdiv_q_ui
    __GMP_DECLSPEC unsigned long int mpz_fdiv_q_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_fdiv_qr __gmpz_fdiv_qr
    __GMP_DECLSPEC void mpz_fdiv_qr __GMP_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_fdiv_qr_ui __gmpz_fdiv_qr_ui
    __GMP_DECLSPEC unsigned long int mpz_fdiv_qr_ui __GMP_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_fdiv_r __gmpz_fdiv_r
    __GMP_DECLSPEC void mpz_fdiv_r __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_fdiv_r_2exp __gmpz_fdiv_r_2exp
    __GMP_DECLSPEC void mpz_fdiv_r_2exp __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_fdiv_r_ui __gmpz_fdiv_r_ui
    __GMP_DECLSPEC unsigned long int mpz_fdiv_r_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_fdiv_ui __gmpz_fdiv_ui
    __GMP_DECLSPEC unsigned long int mpz_fdiv_ui __GMP_PROTO ((mpz_srcptr, unsigned long int)) __GMP_ATTRIBUTE_PURE;

#define mpz_fib_ui __gmpz_fib_ui
    __GMP_DECLSPEC void mpz_fib_ui __GMP_PROTO ((mpz_ptr, unsigned long int));

#define mpz_fib2_ui __gmpz_fib2_ui
    __GMP_DECLSPEC void mpz_fib2_ui __GMP_PROTO ((mpz_ptr, mpz_ptr, unsigned long int));

#define mpz_fits_sint_p __gmpz_fits_sint_p
    __GMP_DECLSPEC int mpz_fits_sint_p __GMP_PROTO ((mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_fits_slong_p __gmpz_fits_slong_p
    __GMP_DECLSPEC int mpz_fits_slong_p __GMP_PROTO ((mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_fits_sshort_p __gmpz_fits_sshort_p
    __GMP_DECLSPEC int mpz_fits_sshort_p __GMP_PROTO ((mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_fits_uint_p __gmpz_fits_uint_p
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpz_fits_uint_p)
    __GMP_DECLSPEC int mpz_fits_uint_p __GMP_PROTO ((mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;
#endif

#define mpz_fits_ulong_p __gmpz_fits_ulong_p
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpz_fits_ulong_p)
    __GMP_DECLSPEC int mpz_fits_ulong_p __GMP_PROTO ((mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;
#endif

#define mpz_fits_ushort_p __gmpz_fits_ushort_p
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpz_fits_ushort_p)
    __GMP_DECLSPEC int mpz_fits_ushort_p __GMP_PROTO ((mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;
#endif

#define mpz_gcd __gmpz_gcd
    __GMP_DECLSPEC void mpz_gcd __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_gcd_ui __gmpz_gcd_ui
    __GMP_DECLSPEC unsigned long int mpz_gcd_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_gcdext __gmpz_gcdext
    __GMP_DECLSPEC void mpz_gcdext __GMP_PROTO ((mpz_ptr, mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_get_d __gmpz_get_d
    __GMP_DECLSPEC double mpz_get_d __GMP_PROTO ((mpz_srcptr)) __GMP_ATTRIBUTE_PURE;

#define mpz_get_d_2exp __gmpz_get_d_2exp
    __GMP_DECLSPEC double mpz_get_d_2exp __GMP_PROTO ((signed long int *, mpz_srcptr));

#define mpz_get_si __gmpz_get_si
    __GMP_DECLSPEC /* signed */ long int mpz_get_si __GMP_PROTO ((mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_get_str __gmpz_get_str
    __GMP_DECLSPEC char *mpz_get_str __GMP_PROTO ((char *, int, mpz_srcptr));

#define mpz_get_ui __gmpz_get_ui
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpz_get_ui)
    __GMP_DECLSPEC unsigned long int mpz_get_ui __GMP_PROTO ((mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;
#endif

#define mpz_getlimbn __gmpz_getlimbn
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpz_getlimbn)
    __GMP_DECLSPEC mp_limb_t mpz_getlimbn __GMP_PROTO ((mpz_srcptr, mp_size_t)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;
#endif

#define mpz_hamdist __gmpz_hamdist
    __GMP_DECLSPEC unsigned long int mpz_hamdist __GMP_PROTO ((mpz_srcptr, mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_import __gmpz_import
    __GMP_DECLSPEC void mpz_import __GMP_PROTO ((mpz_ptr, size_t, int, size_t, int, size_t, __gmp_const void *));

#define mpz_init __gmpz_init
    __GMP_DECLSPEC void mpz_init __GMP_PROTO ((mpz_ptr));

#define mpz_init2 __gmpz_init2
    __GMP_DECLSPEC void mpz_init2 __GMP_PROTO ((mpz_ptr, unsigned long));

#define mpz_init_set __gmpz_init_set
    __GMP_DECLSPEC void mpz_init_set __GMP_PROTO ((mpz_ptr, mpz_srcptr));

#define mpz_init_set_d __gmpz_init_set_d
    __GMP_DECLSPEC void mpz_init_set_d __GMP_PROTO ((mpz_ptr, double));

#define mpz_init_set_si __gmpz_init_set_si
    __GMP_DECLSPEC void mpz_init_set_si __GMP_PROTO ((mpz_ptr, signed long int));

#define mpz_init_set_str __gmpz_init_set_str
    __GMP_DECLSPEC int mpz_init_set_str __GMP_PROTO ((mpz_ptr, __gmp_const char *, int));

#define mpz_init_set_ui __gmpz_init_set_ui
    __GMP_DECLSPEC void mpz_init_set_ui __GMP_PROTO ((mpz_ptr, unsigned long int));

#define mpz_inp_raw __gmpz_inp_raw
#ifdef _GMP_H_HAVE_FILE
    __GMP_DECLSPEC size_t mpz_inp_raw __GMP_PROTO ((mpz_ptr, FILE *));
#endif

#define mpz_inp_str __gmpz_inp_str
#ifdef _GMP_H_HAVE_FILE
    __GMP_DECLSPEC size_t mpz_inp_str __GMP_PROTO ((mpz_ptr, FILE *, int));
#endif

#define mpz_invert __gmpz_invert
    __GMP_DECLSPEC int mpz_invert __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_ior __gmpz_ior
    __GMP_DECLSPEC void mpz_ior __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_jacobi __gmpz_jacobi
    __GMP_DECLSPEC int mpz_jacobi __GMP_PROTO ((mpz_srcptr, mpz_srcptr)) __GMP_ATTRIBUTE_PURE;

#define mpz_kronecker mpz_jacobi  /* 别名 */

#define mpz_kronecker_si __gmpz_kronecker_si
    __GMP_DECLSPEC int mpz_kronecker_si __GMP_PROTO ((mpz_srcptr, long)) __GMP_ATTRIBUTE_PURE;

#define mpz_kronecker_ui __gmpz_kronecker_ui
    __GMP_DECLSPEC int mpz_kronecker_ui __GMP_PROTO ((mpz_srcptr, unsigned long)) __GMP_ATTRIBUTE_PURE;

#define mpz_si_kronecker __gmpz_si_kronecker
    __GMP_DECLSPEC int mpz_si_kronecker __GMP_PROTO ((long, mpz_srcptr)) __GMP_ATTRIBUTE_PURE;

#define mpz_ui_kronecker __gmpz_ui_kronecker
    __GMP_DECLSPEC int mpz_ui_kronecker __GMP_PROTO ((unsigned long, mpz_srcptr)) __GMP_ATTRIBUTE_PURE;

#define mpz_lcm __gmpz_lcm
    __GMP_DECLSPEC void mpz_lcm __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_lcm_ui __gmpz_lcm_ui
    __GMP_DECLSPEC void mpz_lcm_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long));

#define mpz_legendre mpz_jacobi  /* 别名 */

#define mpz_lucnum_ui __gmpz_lucnum_ui
    __GMP_DECLSPEC void mpz_lucnum_ui __GMP_PROTO ((mpz_ptr, unsigned long int));

#define mpz_lucnum2_ui __gmpz_lucnum2_ui
    __GMP_DECLSPEC void mpz_lucnum2_ui __GMP_PROTO ((mpz_ptr, mpz_ptr, unsigned long int));

#define mpz_millerrabin __gmpz_millerrabin
    __GMP_DECLSPEC int mpz_millerrabin __GMP_PROTO ((mpz_srcptr, int)) __GMP_ATTRIBUTE_PURE;

#define mpz_mod __gmpz_mod
    __GMP_DECLSPEC void mpz_mod __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_mod_ui mpz_fdiv_r_ui /* 与fdiv_r相同，因为除数无符号 */

#define mpz_mul __gmpz_mul
    __GMP_DECLSPEC void mpz_mul __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_mul_2exp __gmpz_mul_2exp
    __GMP_DECLSPEC void mpz_mul_2exp __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_mul_si __gmpz_mul_si
    __GMP_DECLSPEC void mpz_mul_si __GMP_PROTO ((mpz_ptr, mpz_srcptr, long int));

#define mpz_mul_ui __gmpz_mul_ui
    __GMP_DECLSPEC void mpz_mul_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_neg __gmpz_neg
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpz_neg)
    __GMP_DECLSPEC void mpz_neg __GMP_PROTO ((mpz_ptr, mpz_srcptr));
#endif

#define mpz_nextprime __gmpz_nextprime
    __GMP_DECLSPEC void mpz_nextprime __GMP_PROTO ((mpz_ptr, mpz_srcptr));

#define mpz_out_raw __gmpz_out_raw
#ifdef _GMP_H_HAVE_FILE
    __GMP_DECLSPEC size_t mpz_out_raw __GMP_PROTO ((FILE *, mpz_srcptr));
#endif

#define mpz_out_str __gmpz_out_str
#ifdef _GMP_H_HAVE_FILE
    __GMP_DECLSPEC size_t mpz_out_str __GMP_PROTO ((FILE *, int, mpz_srcptr));
#endif

#define mpz_perfect_power_p __gmpz_perfect_power_p
    __GMP_DECLSPEC int mpz_perfect_power_p __GMP_PROTO ((mpz_srcptr)) __GMP_ATTRIBUTE_PURE;

#define mpz_perfect_square_p __gmpz_perfect_square_p
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpz_perfect_square_p)
    __GMP_DECLSPEC int mpz_perfect_square_p __GMP_PROTO ((mpz_srcptr)) __GMP_ATTRIBUTE_PURE;
#endif

#define mpz_popcount __gmpz_popcount
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpz_popcount)
    __GMP_DECLSPEC unsigned long int mpz_popcount __GMP_PROTO ((mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;
#endif

#define mpz_pow_ui __gmpz_pow_ui
    __GMP_DECLSPEC void mpz_pow_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_powm __gmpz_powm
    __GMP_DECLSPEC void mpz_powm __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr, mpz_srcptr));

#define mpz_powm_ui __gmpz_powm_ui
    __GMP_DECLSPEC void mpz_powm_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int, mpz_srcptr));

#define mpz_probab_prime_p __gmpz_probab_prime_p
    __GMP_DECLSPEC int mpz_probab_prime_p __GMP_PROTO ((mpz_srcptr, int)) __GMP_ATTRIBUTE_PURE;

#define mpz_random __gmpz_random
    __GMP_DECLSPEC void mpz_random __GMP_PROTO ((mpz_ptr, mp_size_t));

#define mpz_random2 __gmpz_random2
    __GMP_DECLSPEC void mpz_random2 __GMP_PROTO ((mpz_ptr, mp_size_t));

#define mpz_realloc2 __gmpz_realloc2
    __GMP_DECLSPEC void mpz_realloc2 __GMP_PROTO ((mpz_ptr, unsigned long));

#define mpz_remove __gmpz_remove
    __GMP_DECLSPEC unsigned long int mpz_remove __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_root __gmpz_root
    __GMP_DECLSPEC int mpz_root __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_rootrem __gmpz_rootrem
    __GMP_DECLSPEC void mpz_rootrem __GMP_PROTO ((mpz_ptr,mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_rrandomb __gmpz_rrandomb
    __GMP_DECLSPEC void mpz_rrandomb __GMP_PROTO ((mpz_ptr, gmp_randstate_t, unsigned long int));

#define mpz_scan0 __gmpz_scan0
    __GMP_DECLSPEC unsigned long int mpz_scan0 __GMP_PROTO ((mpz_srcptr, unsigned long int)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_scan1 __gmpz_scan1
    __GMP_DECLSPEC unsigned long int mpz_scan1 __GMP_PROTO ((mpz_srcptr, unsigned long int)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_set __gmpz_set
    __GMP_DECLSPEC void mpz_set __GMP_PROTO ((mpz_ptr, mpz_srcptr));

#define mpz_set_d __gmpz_set_d
    __GMP_DECLSPEC void mpz_set_d __GMP_PROTO ((mpz_ptr, double));

#define mpz_set_f __gmpz_set_f
    __GMP_DECLSPEC void mpz_set_f __GMP_PROTO ((mpz_ptr, mpf_srcptr));

#define mpz_set_q __gmpz_set_q
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpz_set_q)
    __GMP_DECLSPEC void mpz_set_q __GMP_PROTO ((mpz_ptr, mpq_srcptr));
#endif

#define mpz_set_si __gmpz_set_si
    __GMP_DECLSPEC void mpz_set_si __GMP_PROTO ((mpz_ptr, signed long int));

#define mpz_set_str __gmpz_set_str
    __GMP_DECLSPEC int mpz_set_str __GMP_PROTO ((mpz_ptr, __gmp_const char *, int));

#define mpz_set_ui __gmpz_set_ui
    __GMP_DECLSPEC void mpz_set_ui __GMP_PROTO ((mpz_ptr, unsigned long int));

#define mpz_setbit __gmpz_setbit
    __GMP_DECLSPEC void mpz_setbit __GMP_PROTO ((mpz_ptr, unsigned long int));

#define mpz_size __gmpz_size
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpz_size)
    __GMP_DECLSPEC size_t mpz_size __GMP_PROTO ((mpz_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;
#endif

#define mpz_sizeinbase __gmpz_sizeinbase
    __GMP_DECLSPEC size_t mpz_sizeinbase __GMP_PROTO ((mpz_srcptr, int)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_sqrt __gmpz_sqrt
    __GMP_DECLSPEC void mpz_sqrt __GMP_PROTO ((mpz_ptr, mpz_srcptr));

#define mpz_sqrtrem __gmpz_sqrtrem
    __GMP_DECLSPEC void mpz_sqrtrem __GMP_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr));

#define mpz_sub __gmpz_sub
    __GMP_DECLSPEC void mpz_sub __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_sub_ui __gmpz_sub_ui
    __GMP_DECLSPEC void mpz_sub_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_ui_sub __gmpz_ui_sub
    __GMP_DECLSPEC void mpz_ui_sub __GMP_PROTO ((mpz_ptr, unsigned long int, mpz_srcptr));

#define mpz_submul __gmpz_submul
    __GMP_DECLSPEC void mpz_submul __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_submul_ui __gmpz_submul_ui
    __GMP_DECLSPEC void mpz_submul_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_swap __gmpz_swap
    __GMP_DECLSPEC void mpz_swap __GMP_PROTO ((mpz_ptr, mpz_ptr)) __GMP_NOTHROW;

#define mpz_tdiv_ui __gmpz_tdiv_ui
    __GMP_DECLSPEC unsigned long int mpz_tdiv_ui __GMP_PROTO ((mpz_srcptr, unsigned long int)) __GMP_ATTRIBUTE_PURE;

#define mpz_tdiv_q __gmpz_tdiv_q
    __GMP_DECLSPEC void mpz_tdiv_q __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_tdiv_q_2exp __gmpz_tdiv_q_2exp
    __GMP_DECLSPEC void mpz_tdiv_q_2exp __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_tdiv_q_ui __gmpz_tdiv_q_ui
    __GMP_DECLSPEC unsigned long int mpz_tdiv_q_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_tdiv_qr __gmpz_tdiv_qr
    __GMP_DECLSPEC void mpz_tdiv_qr __GMP_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_tdiv_qr_ui __gmpz_tdiv_qr_ui
    __GMP_DECLSPEC unsigned long int mpz_tdiv_qr_ui __GMP_PROTO ((mpz_ptr, mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_tdiv_r __gmpz_tdiv_r
    __GMP_DECLSPEC void mpz_tdiv_r __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));

#define mpz_tdiv_r_2exp __gmpz_tdiv_r_2exp
    __GMP_DECLSPEC void mpz_tdiv_r_2exp __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_tdiv_r_ui __gmpz_tdiv_r_ui
    __GMP_DECLSPEC unsigned long int mpz_tdiv_r_ui __GMP_PROTO ((mpz_ptr, mpz_srcptr, unsigned long int));

#define mpz_tstbit __gmpz_tstbit
    __GMP_DECLSPEC int mpz_tstbit __GMP_PROTO ((mpz_srcptr, unsigned long int)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpz_ui_pow_ui __gmpz_ui_pow_ui
    __GMP_DECLSPEC void mpz_ui_pow_ui __GMP_PROTO ((mpz_ptr, unsigned long int, unsigned long int));

#define mpz_urandomb __gmpz_urandomb
    __GMP_DECLSPEC void mpz_urandomb __GMP_PROTO ((mpz_ptr, gmp_randstate_t, unsigned long int));

#define mpz_urandomm __gmpz_urandomm
    __GMP_DECLSPEC void mpz_urandomm __GMP_PROTO ((mpz_ptr, gmp_randstate_t, mpz_srcptr));

#define mpz_xor __gmpz_xor
#define mpz_eor __gmpz_xor
    __GMP_DECLSPEC void mpz_xor __GMP_PROTO ((mpz_ptr, mpz_srcptr, mpz_srcptr));


/**************** 有理数（即Q）例程 ****************/

#define mpq_abs __gmpq_abs
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpq_abs)
    __GMP_DECLSPEC void mpq_abs __GMP_PROTO ((mpq_ptr, mpq_srcptr));
#endif

#define mpq_add __gmpq_add
    __GMP_DECLSPEC void mpq_add __GMP_PROTO ((mpq_ptr, mpq_srcptr, mpq_srcptr));

#define mpq_canonicalize __gmpq_canonicalize
    __GMP_DECLSPEC void mpq_canonicalize __GMP_PROTO ((mpq_ptr));

#define mpq_clear __gmpq_clear
    __GMP_DECLSPEC void mpq_clear __GMP_PROTO ((mpq_ptr));

#define mpq_cmp __gmpq_cmp
    __GMP_DECLSPEC int mpq_cmp __GMP_PROTO ((mpq_srcptr, mpq_srcptr)) __GMP_ATTRIBUTE_PURE;

#define _mpq_cmp_si __gmpq_cmp_si
    __GMP_DECLSPEC int _mpq_cmp_si __GMP_PROTO ((mpq_srcptr, long, unsigned long)) __GMP_ATTRIBUTE_PURE;

#define _mpq_cmp_ui __gmpq_cmp_ui
    __GMP_DECLSPEC int _mpq_cmp_ui __GMP_PROTO ((mpq_srcptr, unsigned long int, unsigned long int)) __GMP_ATTRIBUTE_PURE;

#define mpq_div __gmpq_div
    __GMP_DECLSPEC void mpq_div __GMP_PROTO ((mpq_ptr, mpq_srcptr, mpq_srcptr));

#define mpq_div_2exp __gmpq_div_2exp
    __GMP_DECLSPEC void mpq_div_2exp __GMP_PROTO ((mpq_ptr, mpq_srcptr, unsigned long));

#define mpq_equal __gmpq_equal
    __GMP_DECLSPEC int mpq_equal __GMP_PROTO ((mpq_srcptr, mpq_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpq_get_num __gmpq_get_num
    __GMP_DECLSPEC void mpq_get_num __GMP_PROTO ((mpz_ptr, mpq_srcptr));

#define mpq_get_den __gmpq_get_den
    __GMP_DECLSPEC void mpq_get_den __GMP_PROTO ((mpz_ptr, mpq_srcptr));

#define mpq_get_d __gmpq_get_d
    __GMP_DECLSPEC double mpq_get_d __GMP_PROTO ((mpq_srcptr)) __GMP_ATTRIBUTE_PURE;

#define mpq_get_str __gmpq_get_str
    __GMP_DECLSPEC char *mpq_get_str __GMP_PROTO ((char *, int, mpq_srcptr));

#define mpq_init __gmpq_init
    __GMP_DECLSPEC void mpq_init __GMP_PROTO ((mpq_ptr));

#define mpq_inp_str __gmpq_inp_str
#ifdef _GMP_H_HAVE_FILE
    __GMP_DECLSPEC size_t mpq_inp_str __GMP_PROTO ((mpq_ptr, FILE *, int));
#endif

#define mpq_inv __gmpq_inv
    __GMP_DECLSPEC void mpq_inv __GMP_PROTO ((mpq_ptr, mpq_srcptr));

#define mpq_mul __gmpq_mul
    __GMP_DECLSPEC void mpq_mul __GMP_PROTO ((mpq_ptr, mpq_srcptr, mpq_srcptr));

#define mpq_mul_2exp __gmpq_mul_2exp
    __GMP_DECLSPEC void mpq_mul_2exp __GMP_PROTO ((mpq_ptr, mpq_srcptr, unsigned long));

#define mpq_neg __gmpq_neg
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpq_neg)
    __GMP_DECLSPEC void mpq_neg __GMP_PROTO ((mpq_ptr, mpq_srcptr));
#endif

#define mpq_out_str __gmpq_out_str
#ifdef _GMP_H_HAVE_FILE
    __GMP_DECLSPEC size_t mpq_out_str __GMP_PROTO ((FILE *, int, mpq_srcptr));
#endif

#define mpq_set __gmpq_set
    __GMP_DECLSPEC void mpq_set __GMP_PROTO ((mpq_ptr, mpq_srcptr));

#define mpq_set_d __gmpq_set_d
    __GMP_DECLSPEC void mpq_set_d __GMP_PROTO ((mpq_ptr, double));

#define mpq_set_den __gmpq_set_den
    __GMP_DECLSPEC void mpq_set_den __GMP_PROTO ((mpq_ptr, mpz_srcptr));

#define mpq_set_f __gmpq_set_f
    __GMP_DECLSPEC void mpq_set_f __GMP_PROTO ((mpq_ptr, mpf_srcptr));

#define mpq_set_num __gmpq_set_num
    __GMP_DECLSPEC void mpq_set_num __GMP_PROTO ((mpq_ptr, mpz_srcptr));

#define mpq_set_si __gmpq_set_si
    __GMP_DECLSPEC void mpq_set_si __GMP_PROTO ((mpq_ptr, signed long int, unsigned long int));

#define mpq_set_str __gmpq_set_str
    __GMP_DECLSPEC int mpq_set_str __GMP_PROTO ((mpq_ptr, __gmp_const char *, int));

#define mpq_set_ui __gmpq_set_ui
    __GMP_DECLSPEC void mpq_set_ui __GMP_PROTO ((mpq_ptr, unsigned long int, unsigned long int));

#define mpq_set_z __gmpq_set_z
    __GMP_DECLSPEC void mpq_set_z __GMP_PROTO ((mpq_ptr, mpz_srcptr));

#define mpq_sub __gmpq_sub
    __GMP_DECLSPEC void mpq_sub __GMP_PROTO ((mpq_ptr, mpq_srcptr, mpq_srcptr));

#define mpq_swap __gmpq_swap
    __GMP_DECLSPEC void mpq_swap __GMP_PROTO ((mpq_ptr, mpq_ptr)) __GMP_NOTHROW;


/**************** 浮点数（即F）例程 ****************/

#define mpf_abs __gmpf_abs
    __GMP_DECLSPEC void mpf_abs __GMP_PROTO ((mpf_ptr, mpf_srcptr));

#define mpf_add __gmpf_add
    __GMP_DECLSPEC void mpf_add __GMP_PROTO ((mpf_ptr, mpf_srcptr, mpf_srcptr));

#define mpf_add_ui __gmpf_add_ui
    __GMP_DECLSPEC void mpf_add_ui __GMP_PROTO ((mpf_ptr, mpf_srcptr, unsigned long int));
#define mpf_ceil __gmpf_ceil
    __GMP_DECLSPEC void mpf_ceil __GMP_PROTO ((mpf_ptr, mpf_srcptr));

#define mpf_clear __gmpf_clear
    __GMP_DECLSPEC void mpf_clear __GMP_PROTO ((mpf_ptr));

#define mpf_cmp __gmpf_cmp
    __GMP_DECLSPEC int mpf_cmp __GMP_PROTO ((mpf_srcptr, mpf_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_cmp_d __gmpf_cmp_d
    __GMP_DECLSPEC int mpf_cmp_d __GMP_PROTO ((mpf_srcptr, double)) __GMP_ATTRIBUTE_PURE;

#define mpf_cmp_si __gmpf_cmp_si
    __GMP_DECLSPEC int mpf_cmp_si __GMP_PROTO ((mpf_srcptr, signed long int)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_cmp_ui __gmpf_cmp_ui
    __GMP_DECLSPEC int mpf_cmp_ui __GMP_PROTO ((mpf_srcptr, unsigned long int)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_div __gmpf_div
    __GMP_DECLSPEC void mpf_div __GMP_PROTO ((mpf_ptr, mpf_srcptr, mpf_srcptr));

#define mpf_div_2exp __gmpf_div_2exp
    __GMP_DECLSPEC void mpf_div_2exp __GMP_PROTO ((mpf_ptr, mpf_srcptr, unsigned long int));

#define mpf_div_ui __gmpf_div_ui
    __GMP_DECLSPEC void mpf_div_ui __GMP_PROTO ((mpf_ptr, mpf_srcptr, unsigned long int));

#define mpf_dump __gmpf_dump
    __GMP_DECLSPEC void mpf_dump __GMP_PROTO ((mpf_srcptr));

#define mpf_eq __gmpf_eq
    __GMP_DECLSPEC int mpf_eq __GMP_PROTO ((mpf_srcptr, mpf_srcptr, unsigned long int)) __GMP_ATTRIBUTE_PURE;

#define mpf_fits_sint_p __gmpf_fits_sint_p
    __GMP_DECLSPEC int mpf_fits_sint_p __GMP_PROTO ((mpf_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_fits_slong_p __gmpf_fits_slong_p
    __GMP_DECLSPEC int mpf_fits_slong_p __GMP_PROTO ((mpf_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_fits_sshort_p __gmpf_fits_sshort_p
    __GMP_DECLSPEC int mpf_fits_sshort_p __GMP_PROTO ((mpf_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_fits_uint_p __gmpf_fits_uint_p
    __GMP_DECLSPEC int mpf_fits_uint_p __GMP_PROTO ((mpf_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_fits_ulong_p __gmpf_fits_ulong_p
    __GMP_DECLSPEC int mpf_fits_ulong_p __GMP_PROTO ((mpf_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_fits_ushort_p __gmpf_fits_ushort_p
    __GMP_DECLSPEC int mpf_fits_ushort_p __GMP_PROTO ((mpf_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_floor __gmpf_floor
    __GMP_DECLSPEC void mpf_floor __GMP_PROTO ((mpf_ptr, mpf_srcptr));

#define mpf_get_d __gmpf_get_d
    __GMP_DECLSPEC double mpf_get_d __GMP_PROTO ((mpf_srcptr)) __GMP_ATTRIBUTE_PURE;

#define mpf_get_d_2exp __gmpf_get_d_2exp
    __GMP_DECLSPEC double mpf_get_d_2exp __GMP_PROTO ((signed long int *, mpf_srcptr));

#define mpf_get_default_prec __gmpf_get_default_prec
    __GMP_DECLSPEC unsigned long int mpf_get_default_prec __GMP_PROTO ((void)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_get_prec __gmpf_get_prec
    __GMP_DECLSPEC unsigned long int mpf_get_prec __GMP_PROTO ((mpf_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_get_si __gmpf_get_si
    __GMP_DECLSPEC long mpf_get_si __GMP_PROTO ((mpf_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_get_str __gmpf_get_str
    __GMP_DECLSPEC char *mpf_get_str __GMP_PROTO ((char *, mp_exp_t *, int, size_t, mpf_srcptr));

#define mpf_get_ui __gmpf_get_ui
    __GMP_DECLSPEC unsigned long mpf_get_ui __GMP_PROTO ((mpf_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_init __gmpf_init
    __GMP_DECLSPEC void mpf_init __GMP_PROTO ((mpf_ptr));

#define mpf_init2 __gmpf_init2
    __GMP_DECLSPEC void mpf_init2 __GMP_PROTO ((mpf_ptr, unsigned long int));

#define mpf_init_set __gmpf_init_set
    __GMP_DECLSPEC void mpf_init_set __GMP_PROTO ((mpf_ptr, mpf_srcptr));

#define mpf_init_set_d __gmpf_init_set_d
    __GMP_DECLSPEC void mpf_init_set_d __GMP_PROTO ((mpf_ptr, double));

#define mpf_init_set_si __gmpf_init_set_si
    __GMP_DECLSPEC void mpf_init_set_si __GMP_PROTO ((mpf_ptr, signed long int));

#define mpf_init_set_str __gmpf_init_set_str
    __GMP_DECLSPEC int mpf_init_set_str __GMP_PROTO ((mpf_ptr, __gmp_const char *, int));

#define mpf_init_set_ui __gmpf_init_set_ui
    __GMP_DECLSPEC void mpf_init_set_ui __GMP_PROTO ((mpf_ptr, unsigned long int));

#define mpf_inp_str __gmpf_inp_str
#ifdef _GMP_H_HAVE_FILE
    __GMP_DECLSPEC size_t mpf_inp_str __GMP_PROTO ((mpf_ptr, FILE *, int));
#endif

#define mpf_integer_p __gmpf_integer_p
    __GMP_DECLSPEC int mpf_integer_p __GMP_PROTO ((mpf_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_mul __gmpf_mul
    __GMP_DECLSPEC void mpf_mul __GMP_PROTO ((mpf_ptr, mpf_srcptr, mpf_srcptr));

#define mpf_mul_2exp __gmpf_mul_2exp
    __GMP_DECLSPEC void mpf_mul_2exp __GMP_PROTO ((mpf_ptr, mpf_srcptr, unsigned long int));

#define mpf_mul_ui __gmpf_mul_ui
    __GMP_DECLSPEC void mpf_mul_ui __GMP_PROTO ((mpf_ptr, mpf_srcptr, unsigned long int));

#define mpf_neg __gmpf_neg
    __GMP_DECLSPEC void mpf_neg __GMP_PROTO ((mpf_ptr, mpf_srcptr));

#define mpf_out_str __gmpf_out_str
#ifdef _GMP_H_HAVE_FILE
    __GMP_DECLSPEC size_t mpf_out_str __GMP_PROTO ((FILE *, int, size_t, mpf_srcptr));
#endif

#define mpf_pow_ui __gmpf_pow_ui
    __GMP_DECLSPEC void mpf_pow_ui __GMP_PROTO ((mpf_ptr, mpf_srcptr, unsigned long int));

#define mpf_random2 __gmpf_random2
    __GMP_DECLSPEC void mpf_random2 __GMP_PROTO ((mpf_ptr, mp_size_t, mp_exp_t));

#define mpf_reldiff __gmpf_reldiff
    __GMP_DECLSPEC void mpf_reldiff __GMP_PROTO ((mpf_ptr, mpf_srcptr, mpf_srcptr));

#define mpf_set __gmpf_set
    __GMP_DECLSPEC void mpf_set __GMP_PROTO ((mpf_ptr, mpf_srcptr));

#define mpf_set_d __gmpf_set_d
    __GMP_DECLSPEC void mpf_set_d __GMP_PROTO ((mpf_ptr, double));

#define mpf_set_default_prec __gmpf_set_default_prec
    __GMP_DECLSPEC void mpf_set_default_prec __GMP_PROTO ((unsigned long int)) __GMP_NOTHROW;

#define mpf_set_prec __gmpf_set_prec
    __GMP_DECLSPEC void mpf_set_prec __GMP_PROTO ((mpf_ptr, unsigned long int));

#define mpf_set_prec_raw __gmpf_set_prec_raw
    __GMP_DECLSPEC void mpf_set_prec_raw __GMP_PROTO ((mpf_ptr, unsigned long int)) __GMP_NOTHROW;

#define mpf_set_q __gmpf_set_q
    __GMP_DECLSPEC void mpf_set_q __GMP_PROTO ((mpf_ptr, mpq_srcptr));

#define mpf_set_si __gmpf_set_si
    __GMP_DECLSPEC void mpf_set_si __GMP_PROTO ((mpf_ptr, signed long int));

#define mpf_set_str __gmpf_set_str
    __GMP_DECLSPEC int mpf_set_str __GMP_PROTO ((mpf_ptr, __gmp_const char *, int));

#define mpf_set_ui __gmpf_set_ui
    __GMP_DECLSPEC void mpf_set_ui __GMP_PROTO ((mpf_ptr, unsigned long int));

#define mpf_set_z __gmpf_set_z
    __GMP_DECLSPEC void mpf_set_z __GMP_PROTO ((mpf_ptr, mpz_srcptr));

#define mpf_size __gmpf_size
    __GMP_DECLSPEC size_t mpf_size __GMP_PROTO ((mpf_srcptr)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpf_sqrt __gmpf_sqrt
    __GMP_DECLSPEC void mpf_sqrt __GMP_PROTO ((mpf_ptr, mpf_srcptr));

#define mpf_sqrt_ui __gmpf_sqrt_ui
    __GMP_DECLSPEC void mpf_sqrt_ui __GMP_PROTO ((mpf_ptr, unsigned long int));

#define mpf_sub __gmpf_sub
    __GMP_DECLSPEC void mpf_sub __GMP_PROTO ((mpf_ptr, mpf_srcptr, mpf_srcptr));

#define mpf_sub_ui __gmpf_sub_ui
    __GMP_DECLSPEC void mpf_sub_ui __GMP_PROTO ((mpf_ptr, mpf_srcptr, unsigned long int));

#define mpf_swap __gmpf_swap
    __GMP_DECLSPEC void mpf_swap __GMP_PROTO ((mpf_ptr, mpf_ptr)) __GMP_NOTHROW;

#define mpf_trunc __gmpf_trunc
    __GMP_DECLSPEC void mpf_trunc __GMP_PROTO ((mpf_ptr, mpf_srcptr));

#define mpf_ui_div __gmpf_ui_div
    __GMP_DECLSPEC void mpf_ui_div __GMP_PROTO ((mpf_ptr, unsigned long int, mpf_srcptr));

#define mpf_ui_sub __gmpf_ui_sub
    __GMP_DECLSPEC void mpf_ui_sub __GMP_PROTO ((mpf_ptr, unsigned long int, mpf_srcptr));

#define mpf_urandomb __gmpf_urandomb
    __GMP_DECLSPEC void mpf_urandomb __GMP_PROTO ((mpf_t, gmp_randstate_t, unsigned long int));


/************ 低级正整数（即N）例程 ************/

/* 这很丑，但我们需要让用户调用到达带前缀的函数 */

#define mpn_add __MPN(add)
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpn_add)
    __GMP_DECLSPEC mp_limb_t mpn_add __GMP_PROTO ((mp_ptr, mp_srcptr, mp_size_t, mp_srcptr,mp_size_t));
#endif

#define mpn_add_1 __MPN(add_1)
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpn_add_1)
    __GMP_DECLSPEC mp_limb_t mpn_add_1 __GMP_PROTO ((mp_ptr, mp_srcptr, mp_size_t, mp_limb_t)) __GMP_NOTHROW;
#endif

#define mpn_add_n __MPN(add_n)
    __GMP_DECLSPEC mp_limb_t mpn_add_n __GMP_PROTO ((mp_ptr, mp_srcptr, mp_srcptr, mp_size_t));

#define mpn_addmul_1 __MPN(addmul_1)
    __GMP_DECLSPEC mp_limb_t mpn_addmul_1 __GMP_PROTO ((mp_ptr, mp_srcptr, mp_size_t, mp_limb_t));

#define mpn_bdivmod __MPN(bdivmod)
    __GMP_DECLSPEC mp_limb_t mpn_bdivmod __GMP_PROTO ((mp_ptr, mp_ptr, mp_size_t, mp_srcptr, mp_size_t, unsigned long int));

#define mpn_cmp __MPN(cmp)
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpn_cmp)
    __GMP_DECLSPEC int mpn_cmp __GMP_PROTO ((mp_srcptr, mp_srcptr, mp_size_t)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;
#endif

#define mpn_divexact_by3(dst,src,size) \
    mpn_divexact_by3c (dst, src, size, __GMP_CAST (mp_limb_t, 0))

#define mpn_divexact_by3c __MPN(divexact_by3c)
        __GMP_DECLSPEC mp_limb_t mpn_divexact_by3c __GMP_PROTO ((mp_ptr, mp_srcptr, mp_size_t, mp_limb_t));

#define mpn_divmod_1(qp,np,nsize,dlimb) \
    mpn_divrem_1 (qp, __GMP_CAST (mp_size_t, 0), np, nsize, dlimb)

#define mpn_divrem __MPN(divrem)
        __GMP_DECLSPEC mp_limb_t mpn_divrem __GMP_PROTO ((mp_ptr, mp_size_t, mp_ptr, mp_size_t, mp_srcptr, mp_size_t));

#define mpn_divrem_1 __MPN(divrem_1)
    __GMP_DECLSPEC mp_limb_t mpn_divrem_1 __GMP_PROTO ((mp_ptr, mp_size_t, mp_srcptr, mp_size_t, mp_limb_t));

#define mpn_divrem_2 __MPN(divrem_2)
    __GMP_DECLSPEC mp_limb_t mpn_divrem_2 __GMP_PROTO ((mp_ptr, mp_size_t, mp_ptr, mp_size_t, mp_srcptr));

#define mpn_gcd __MPN(gcd)
    __GMP_DECLSPEC mp_size_t mpn_gcd __GMP_PROTO ((mp_ptr, mp_ptr, mp_size_t, mp_ptr, mp_size_t));

#define mpn_gcd_1 __MPN(gcd_1)
    __GMP_DECLSPEC mp_limb_t mpn_gcd_1 __GMP_PROTO ((mp_srcptr, mp_size_t, mp_limb_t)) __GMP_ATTRIBUTE_PURE;

#define mpn_gcdext __MPN(gcdext)
    __GMP_DECLSPEC mp_size_t mpn_gcdext __GMP_PROTO ((mp_ptr, mp_ptr, mp_size_t *, mp_ptr, mp_size_t, mp_ptr, mp_size_t));

#define mpn_get_str __MPN(get_str)
    __GMP_DECLSPEC size_t mpn_get_str __GMP_PROTO ((unsigned char *, int, mp_ptr, mp_size_t));

#define mpn_hamdist __MPN(hamdist)
    __GMP_DECLSPEC unsigned long int mpn_hamdist __GMP_PROTO ((mp_srcptr, mp_srcptr, mp_size_t)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpn_lshift __MPN(lshift)
    __GMP_DECLSPEC mp_limb_t mpn_lshift __GMP_PROTO ((mp_ptr, mp_srcptr, mp_size_t, unsigned int));

#define mpn_mod_1 __MPN(mod_1)
    __GMP_DECLSPEC mp_limb_t mpn_mod_1 __GMP_PROTO ((mp_srcptr, mp_size_t, mp_limb_t)) __GMP_ATTRIBUTE_PURE;

#define mpn_mul __MPN(mul)
    __GMP_DECLSPEC mp_limb_t mpn_mul __GMP_PROTO ((mp_ptr, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t));

#define mpn_mul_1 __MPN(mul_1)
    __GMP_DECLSPEC mp_limb_t mpn_mul_1 __GMP_PROTO ((mp_ptr, mp_srcptr, mp_size_t, mp_limb_t));

#define mpn_mul_n __MPN(mul_n)
    __GMP_DECLSPEC void mpn_mul_n __GMP_PROTO ((mp_ptr, mp_srcptr, mp_srcptr, mp_size_t));

#define mpn_perfect_square_p __MPN(perfect_square_p)
    __GMP_DECLSPEC int mpn_perfect_square_p __GMP_PROTO ((mp_srcptr, mp_size_t)) __GMP_ATTRIBUTE_PURE;

#define mpn_popcount __MPN(popcount)
    __GMP_DECLSPEC unsigned long int mpn_popcount __GMP_PROTO ((mp_srcptr, mp_size_t)) __GMP_NOTHROW __GMP_ATTRIBUTE_PURE;

#define mpn_pow_1 __MPN(pow_1)
    __GMP_DECLSPEC mp_size_t mpn_pow_1 __GMP_PROTO ((mp_ptr, mp_srcptr, mp_size_t, mp_limb_t, mp_ptr));

/* 现在未文档化，但保留在此以保持向上兼容性 */
#define mpn_preinv_mod_1 __MPN(preinv_mod_1)
    __GMP_DECLSPEC mp_limb_t mpn_preinv_mod_1 __GMP_PROTO ((mp_srcptr, mp_size_t, mp_limb_t, mp_limb_t)) __GMP_ATTRIBUTE_PURE;

#define mpn_random __MPN(random)
    __GMP_DECLSPEC void mpn_random __GMP_PROTO ((mp_ptr, mp_size_t));

#define mpn_random2 __MPN(random2)
    __GMP_DECLSPEC void mpn_random2 __GMP_PROTO ((mp_ptr, mp_size_t));

#define mpn_rshift __MPN(rshift)
    __GMP_DECLSPEC mp_limb_t mpn_rshift __GMP_PROTO ((mp_ptr, mp_srcptr, mp_size_t, unsigned int));

#define mpn_scan0 __MPN(scan0)
    __GMP_DECLSPEC unsigned long int mpn_scan0 __GMP_PROTO ((mp_srcptr, unsigned long int)) __GMP_ATTRIBUTE_PURE;

#define mpn_scan1 __MPN(scan1)
    __GMP_DECLSPEC unsigned long int mpn_scan1 __GMP_PROTO ((mp_srcptr, unsigned long int)) __GMP_ATTRIBUTE_PURE;

#define mpn_set_str __MPN(set_str)
    __GMP_DECLSPEC mp_size_t mpn_set_str __GMP_PROTO ((mp_ptr, __gmp_const unsigned char *, size_t, int));

#define mpn_sqrtrem __MPN(sqrtrem)
    __GMP_DECLSPEC mp_size_t mpn_sqrtrem __GMP_PROTO ((mp_ptr, mp_ptr, mp_srcptr, mp_size_t));

#define mpn_sub __MPN(sub)
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpn_sub)
    __GMP_DECLSPEC mp_limb_t mpn_sub __GMP_PROTO ((mp_ptr, mp_srcptr, mp_size_t, mp_srcptr,mp_size_t));
#endif

#define mpn_sub_1 __MPN(sub_1)
#if __GMP_INLINE_PROTOTYPES || defined (__GMP_FORCE_mpn_sub_1)
    __GMP_DECLSPEC mp_limb_t mpn_sub_1 __GMP_PROTO ((mp_ptr, mp_srcptr, mp_size_t, mp_limb_t)) __GMP_NOTHROW;
#endif

#define mpn_sub_n __MPN(sub_n)
    __GMP_DECLSPEC mp_limb_t mpn_sub_n __GMP_PROTO ((mp_ptr, mp_srcptr, mp_srcptr, mp_size_t));

#define mpn_submul_1 __MPN(submul_1)
    __GMP_DECLSPEC mp_limb_t mpn_submul_1 __GMP_PROTO ((mp_ptr, mp_srcptr, mp_size_t, mp_limb_t));

#define mpn_tdiv_qr __MPN(tdiv_qr)
    __GMP_DECLSPEC void mpn_tdiv_qr __GMP_PROTO ((mp_ptr, mp_ptr, mp_size_t, mp_srcptr, mp_size_t, mp_srcptr, mp_size_t));


/**************** mpz内联函数 ****************/

/* 以下尽可能以内联形式提供，但也始终作为库函数存在，以保持二进制兼容性。

   在gmp内部，通常不依赖此内联，因为它
   并非对所有编译器都完成，而如果某物值得内联，
   那么值得始终安排。

   这里有两种内联样式。当内联需要与库版本相同的代码时，
   则__GMP_FORCE_foo安排发射该代码并抑制__GMP_EXTERN_INLINE
   指令，例如mpz_fits_uint_p。当内联需要与库版本不同的代码时，
   则__GMP_FORCE_foo安排抑制内联，例如mpz_abs */

#if defined (__GMP_EXTERN_INLINE) && ! defined (__GMP_FORCE_mpz_abs)
    __GMP_EXTERN_INLINE void
    mpz_abs (mpz_ptr __gmp_w, mpz_srcptr __gmp_u)
    {
        if (__gmp_w != __gmp_u)
            mpz_set (__gmp_w, __gmp_u);
        __gmp_w->_mp_size = __GMP_ABS (__gmp_w->_mp_size);
    }
#endif

#if GMP_NAIL_BITS == 0
#define __GMPZ_FITS_UTYPE_P(z,maxval)                    \
    mp_size_t  __gmp_n = z->_mp_size;                    \
        mp_ptr  __gmp_p = z->_mp_d;                        \
        return (__gmp_n == 0 || (__gmp_n == 1 && __gmp_p[0] <= maxval));
#else
#define __GMPZ_FITS_UTYPE_P(z,maxval)                    \
mp_size_t  __gmp_n = z->_mp_size;                    \
    mp_ptr  __gmp_p = z->_mp_d;                        \
    return (__gmp_n == 0 || (__gmp_n == 1 && __gmp_p[0] <= maxval)    \
            || (__gmp_n == 2 && __gmp_p[1] <= ((mp_limb_t) maxval >> GMP_NUMB_BITS)));
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpz_fits_uint_p)
#if ! defined (__GMP_FORCE_mpz_fits_uint_p)
    __GMP_EXTERN_INLINE
#endif
        int
        mpz_fits_uint_p (mpz_srcptr __gmp_z) __GMP_NOTHROW
    {
        __GMPZ_FITS_UTYPE_P (__gmp_z, __GMP_UINT_MAX);
    }
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpz_fits_ulong_p)
#if ! defined (__GMP_FORCE_mpz_fits_ulong_p)
    __GMP_EXTERN_INLINE
#endif
        int
        mpz_fits_ulong_p (mpz_srcptr __gmp_z) __GMP_NOTHROW
    {
        __GMPZ_FITS_UTYPE_P (__gmp_z, __GMP_ULONG_MAX);
    }
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpz_fits_ushort_p)
#if ! defined (__GMP_FORCE_mpz_fits_ushort_p)
    __GMP_EXTERN_INLINE
#endif
        int
        mpz_fits_ushort_p (mpz_srcptr __gmp_z) __GMP_NOTHROW
    {
        __GMPZ_FITS_UTYPE_P (__gmp_z, __GMP_USHRT_MAX);
    }
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpz_get_ui)
#if ! defined (__GMP_FORCE_mpz_get_ui)
    __GMP_EXTERN_INLINE
#endif
        unsigned long
        mpz_get_ui (mpz_srcptr __gmp_z) __GMP_NOTHROW
    {
        mp_ptr __gmp_p = __gmp_z->_mp_d;
        mp_size_t __gmp_n = __gmp_z->_mp_size;
        mp_limb_t __gmp_l = __gmp_p[0];
        /* 这是"#if"而不是普通的"if"，以避免gcc关于"<< GMP_NUMB_BITS"超过类型大小的警告，
     并避免Borland C++ 6.0关于类似"__GMP_ULONG_MAX < GMP_NUMB_MASK"的条件始终为真的警告 */
#if GMP_NAIL_BITS == 0 || defined (_LONG_LONG_LIMB)
        /* limb==long且无钉子，或limb==longlong，一个limb足够 */
        return (__gmp_n != 0 ? __gmp_l : 0);
#else
        /* limb==long且有钉子，需要两个limbs（如果可用） */
        __gmp_n = __GMP_ABS (__gmp_n);
        if (__gmp_n <= 1)
            return (__gmp_n != 0 ? __gmp_l : 0);
        else
            return __gmp_l + (__gmp_p[1] << GMP_NUMB_BITS);
#endif
    }
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpz_getlimbn)
#if ! defined (__GMP_FORCE_mpz_getlimbn)
    __GMP_EXTERN_INLINE
#endif
        mp_limb_t
        mpz_getlimbn (mpz_srcptr __gmp_z, mp_size_t __gmp_n) __GMP_NOTHROW
    {
        mp_limb_t  __gmp_result = 0;
        if (__GMP_LIKELY (__gmp_n >= 0 && __gmp_n < __GMP_ABS (__gmp_z->_mp_size)))
            __gmp_result = __gmp_z->_mp_d[__gmp_n];
        return __gmp_result;
    }
#endif

#if defined (__GMP_EXTERN_INLINE) && ! defined (__GMP_FORCE_mpz_neg)
    __GMP_EXTERN_INLINE void
    mpz_neg (mpz_ptr __gmp_w, mpz_srcptr __gmp_u)
    {
        if (__gmp_w != __gmp_u)
            mpz_set (__gmp_w, __gmp_u);
        __gmp_w->_mp_size = - __gmp_w->_mp_size;
    }
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpz_perfect_square_p)
#if ! defined (__GMP_FORCE_mpz_perfect_square_p)
    __GMP_EXTERN_INLINE
#endif
        int
        mpz_perfect_square_p (mpz_srcptr __gmp_a)
    {
        mp_size_t __gmp_asize;
        int       __gmp_result;

        __gmp_asize = __gmp_a->_mp_size;
        __gmp_result = (__gmp_asize >= 0);  /* 零是平方，负数不是 */
        if (__GMP_LIKELY (__gmp_asize > 0))
            __gmp_result = mpn_perfect_square_p (__gmp_a->_mp_d, __gmp_asize);
        return __gmp_result;
    }
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpz_popcount)
#if ! defined (__GMP_FORCE_mpz_popcount)
    __GMP_EXTERN_INLINE
#endif
        unsigned long
        mpz_popcount (mpz_srcptr __gmp_u) __GMP_NOTHROW
    {
        mp_size_t      __gmp_usize;
        unsigned long  __gmp_result;

        __gmp_usize = __gmp_u->_mp_size;
        __gmp_result = (__gmp_usize < 0 ? __GMP_ULONG_MAX : 0);
        if (__GMP_LIKELY (__gmp_usize > 0))
            __gmp_result =  mpn_popcount (__gmp_u->_mp_d, __gmp_usize);
        return __gmp_result;
    }
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpz_set_q)
#if ! defined (__GMP_FORCE_mpz_set_q)
    __GMP_EXTERN_INLINE
#endif
        void
        mpz_set_q (mpz_ptr __gmp_w, mpq_srcptr __gmp_u)
    {
        mpz_tdiv_q (__gmp_w, mpq_numref (__gmp_u), mpq_denref (__gmp_u));
    }
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpz_size)
#if ! defined (__GMP_FORCE_mpz_size)
    __GMP_EXTERN_INLINE
#endif
        size_t
        mpz_size (mpz_srcptr __gmp_z) __GMP_NOTHROW
    {
        return __GMP_ABS (__gmp_z->_mp_size);
    }
#endif


/**************** mpq内联函数 ****************/

#if defined (__GMP_EXTERN_INLINE) && ! defined (__GMP_FORCE_mpq_abs)
    __GMP_EXTERN_INLINE void
    mpq_abs (mpq_ptr __gmp_w, mpq_srcptr __gmp_u)
    {
        if (__gmp_w != __gmp_u)
            mpq_set (__gmp_w, __gmp_u);
        __gmp_w->_mp_num._mp_size = __GMP_ABS (__gmp_w->_mp_num._mp_size);
    }
#endif

#if defined (__GMP_EXTERN_INLINE) && ! defined (__GMP_FORCE_mpq_neg)
    __GMP_EXTERN_INLINE void
    mpq_neg (mpq_ptr __gmp_w, mpq_srcptr __gmp_u)
    {
        if (__gmp_w != __gmp_u)
            mpq_set (__gmp_w, __gmp_u);
        __gmp_w->_mp_num._mp_size = - __gmp_w->_mp_num._mp_size;
    }
#endif


/**************** mpn内联函数 ****************/

/* 以下关于__GMPN_ADD_1的注释也适用于这里。

   测试FUNCTION返回0应能很好预测。如果假设{yp,ysize}通常具有随机位数，
   则高位limb不会满，进位发生将远少于50%的时间。

   ysize==0不是文档化功能，但在内部一些地方使用。

   最后生成cout可防止它在计算主要部分期间占用寄存器，
   尽管gcc（截至3.0）在"if (mpn_add (...))"上似乎无法将
   条件语句的真假分支移动到生成cout的两个位置 */

#define __GMPN_AORS(cout, wp, xp, xsize, yp, ysize, FUNCTION, TEST)     \
    do {                                                                  \
            mp_size_t  __gmp_i;                                                 \
            mp_limb_t  __gmp_x;                                                 \
                                                                        \
            /* ASSERT ((ysize) >= 0); */                                        \
            /* ASSERT ((xsize) >= (ysize)); */                                  \
            /* ASSERT (MPN_SAME_OR_SEPARATE2_P (wp, xsize, xp, xsize)); */      \
            /* ASSERT (MPN_SAME_OR_SEPARATE2_P (wp, xsize, yp, ysize)); */      \
                                                                        \
            __gmp_i = (ysize);                                                  \
            if (__gmp_i != 0)                                                   \
        {                                                                 \
                if (FUNCTION (wp, xp, yp, __gmp_i))                             \
            {                                                             \
                    do                                                          \
                {                                                         \
                        if (__gmp_i >= (xsize))                                 \
                    {                                                     \
                            (cout) = 1;                                         \
                            goto __gmp_done;                                    \
                    }                                                     \
                        __gmp_x = (xp)[__gmp_i];                                \
                }                                                         \
                    while (TEST);                                               \
            }                                                             \
        }                                                                 \
            if ((wp) != (xp))                                                   \
            __GMPN_COPY_REST (wp, xp, xsize, __gmp_i);                        \
            (cout) = 0;                                                         \
            __gmp_done:                                                           \
            ;                                                                   \
    } while (0)

#define __GMPN_ADD(cout, wp, xp, xsize, yp, ysize)              \
        __GMPN_AORS (cout, wp, xp, xsize, yp, ysize, mpn_add_n,       \
                    (((wp)[__gmp_i++] = (__gmp_x + 1) & GMP_NUMB_MASK) == 0))
#define __GMPN_SUB(cout, wp, xp, xsize, yp, ysize)              \
        __GMPN_AORS (cout, wp, xp, xsize, yp, ysize, mpn_sub_n,       \
                    (((wp)[__gmp_i++] = (__gmp_x - 1) & GMP_NUMB_MASK), __gmp_x == 0))


/* 使用__gmp_i索引旨在确保编译时src==dst对编译器保持清晰明了，
   这样__GMPN_COPY_REST可以消失，并且load/add/store有机会在CISC CPU上
   变为读-修改-写。

   替代方案：

   使用指针对而不是索引是可能的，但gcc无法识别这种情况下的编译时src==dst，
   即使指针或多或少一起递增。其他编译器很可能有类似的困难。

   gcc可以使用"if (__builtin_constant_p(src==dst) && src==dst)"或
   类似方法来检测编译时src==dst。这在gcc 2.95.x上工作良好，
   在gcc 3.0上不好，其中__builtin_constant_p(p==p)对于指针p似乎总是false。
   但当前代码形式对于src==dst似乎已经足够好。

   gcc在x86上通常不提供特别好的标志处理来进行进位/借位检测。
   想要一些多指令asm块来帮助它是诱人的，并且已经尝试过，
   但事实上只有几条指令可以节省，并且任何增益都很容易因设置asm的寄存器杂耍而丢失 */

#if GMP_NAIL_BITS == 0
#define __GMPN_AORS_1(cout, dst, src, n, v, OP, CB)        \
            do {                                \
            mp_size_t  __gmp_i;                        \
            mp_limb_t  __gmp_x, __gmp_r;                                \
                                \
            /* ASSERT ((n) >= 1); */                    \
            /* ASSERT (MPN_SAME_OR_SEPARATE_P (dst, src, n)); */    \
                                \
            __gmp_x = (src)[0];                        \
            __gmp_r = __gmp_x OP (v);                                   \
            (dst)[0] = __gmp_r;                        \
            if (CB (__gmp_r, __gmp_x, (v)))                             \
        {                                \
                (cout) = 1;                        \
                for (__gmp_i = 1; __gmp_i < (n);)                       \
            {                            \
                    __gmp_x = (src)[__gmp_i];                           \
                    __gmp_r = __gmp_x OP 1;                             \
                    (dst)[__gmp_i] = __gmp_r;                           \
                    ++__gmp_i;                        \
                    if (!CB (__gmp_r, __gmp_x, 1))                      \
                {                            \
                        if ((src) != (dst))                \
                        __GMPN_COPY_REST (dst, src, n, __gmp_i);      \
                        (cout) = 0;                    \
                        break;                      \
                }                            \
            }                            \
        }                                \
            else                            \
        {                                \
                if ((src) != (dst))                    \
                __GMPN_COPY_REST (dst, src, n, 1);            \
                (cout) = 0;                        \
        }                                \
    } while (0)
#endif

#if GMP_NAIL_BITS >= 1
#define __GMPN_AORS_1(cout, dst, src, n, v, OP, CB)        \
        do {                                \
                mp_size_t  __gmp_i;                        \
                mp_limb_t  __gmp_x, __gmp_r;                \
                                \
                /* ASSERT ((n) >= 1); */                    \
                /* ASSERT (MPN_SAME_OR_SEPARATE_P (dst, src, n)); */    \
                                \
                __gmp_x = (src)[0];                        \
                __gmp_r = __gmp_x OP (v);                    \
                (dst)[0] = __gmp_r & GMP_NUMB_MASK;                \
                if (__gmp_r >> GMP_NUMB_BITS != 0)                \
            {                                \
                    (cout) = 1;                        \
                    for (__gmp_i = 1; __gmp_i < (n);)            \
                {                            \
                        __gmp_x = (src)[__gmp_i];                \
                        __gmp_r = __gmp_x OP 1;                \
                        (dst)[__gmp_i] = __gmp_r & GMP_NUMB_MASK;        \
                        ++__gmp_i;                        \
                        if (__gmp_r >> GMP_NUMB_BITS == 0)            \
                    {                            \
                            if ((src) != (dst))                \
                            __GMPN_COPY_REST (dst, src, n, __gmp_i);    \
                            (cout) = 0;                    \
                            break;                      \
                    }                            \
                }                            \
            }                                \
                else                            \
            {                                \
                    if ((src) != (dst))                    \
                    __GMPN_COPY_REST (dst, src, n, 1);            \
                    (cout) = 0;                        \
            }                                \
        } while (0)
#endif

#define __GMPN_ADDCB(r,x,y) ((r) < (y))
#define __GMPN_SUBCB(r,x,y) ((x) < (y))

#define __GMPN_ADD_1(cout, dst, src, n, v)         \
            __GMPN_AORS_1(cout, dst, src, n, v, +, __GMPN_ADDCB)
#define __GMPN_SUB_1(cout, dst, src, n, v)         \
            __GMPN_AORS_1(cout, dst, src, n, v, -, __GMPN_SUBCB)


/* 比较{xp,size}和{yp,size}，将"result"设为正、零或负。
   size==0是允许的。在随机数据上，通常只需要检查一个limb即可得到结果，
   因此值得内联 */
#define __GMPN_CMP(result, xp, yp, size)                                \
                do {                                                                  \
                mp_size_t  __gmp_i;                                                 \
                mp_limb_t  __gmp_x, __gmp_y;                                        \
                                                                        \
                /* ASSERT ((size) >= 0); */                                         \
                                                                        \
                (result) = 0;                                                       \
                __gmp_i = (size);                                                   \
                while (--__gmp_i >= 0)                                              \
            {                                                                 \
                    __gmp_x = (xp)[__gmp_i];                                        \
                    __gmp_y = (yp)[__gmp_i];                                        \
                    if (__gmp_x != __gmp_y)                                         \
                {                                                             \
                        /* 不能使用__gmp_x - __gmp_y，可能溢出"int" */   \
                        (result) = (__gmp_x > __gmp_y ? 1 : -1);                    \
                        break;                                                      \
                }                                                             \
            }                                                                 \
        } while (0)


#if defined (__GMPN_COPY) && ! defined (__GMPN_COPY_REST)
#define __GMPN_COPY_REST(dst, src, size, start)                 \
        do {                                                          \
                /* ASSERT ((start) >= 0); */                                \
                /* ASSERT ((start) <= (size)); */                           \
                __GMPN_COPY ((dst)+(start), (src)+(start), (size)-(start)); \
        } while (0)
#endif

/* 从"start"开始复制{src,size}到{dst,size}。这旨在保持
   __GMPN_ADD_1、__GMPN_ADD等的dst[j]和src[j]索引简洁明了 */
#if ! defined (__GMPN_COPY_REST)
#define __GMPN_COPY_REST(dst, src, size, start)                 \
            do {                                                          \
                mp_size_t __gmp_j;                                          \
                /* ASSERT ((size) >= 0); */                                 \
                /* ASSERT ((start) >= 0); */                                \
                /* ASSERT ((start) <= (size)); */                           \
                /* ASSERT (MPN_SAME_OR_SEPARATE_P (dst, src, size)); */     \
                __GMP_CRAY_Pragma ("_CRI ivdep");                           \
                for (__gmp_j = (start); __gmp_j < (size); __gmp_j++)        \
                (dst)[__gmp_j] = (src)[__gmp_j];                          \
        } while (0)
#endif

/* 增强：使用gmp-impl.h中的一些更智能的代码。如果有一个本地版本，
   并且我们不介意要求其二进制兼容性（在使用它的目标上），
   则可能使用mpn_copyi */

#if ! defined (__GMPN_COPY)
#define __GMPN_COPY(dst, src, size)   __GMPN_COPY_REST (dst, src, size, 0)
#endif


#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpn_add)
#if ! defined (__GMP_FORCE_mpn_add)
        __GMP_EXTERN_INLINE
#endif
            mp_limb_t
            mpn_add (mp_ptr __gmp_wp, mp_srcptr __gmp_xp, mp_size_t __gmp_xsize, mp_srcptr __gmp_yp, mp_size_t __gmp_ysize)
        {
            mp_limb_t  __gmp_c;
            __GMPN_ADD (__gmp_c, __gmp_wp, __gmp_xp, __gmp_xsize, __gmp_yp, __gmp_ysize);
            return __gmp_c;
        }
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpn_add_1)
#if ! defined (__GMP_FORCE_mpn_add_1)
    __GMP_EXTERN_INLINE
#endif
        mp_limb_t
        mpn_add_1 (mp_ptr __gmp_dst, mp_srcptr __gmp_src, mp_size_t __gmp_size, mp_limb_t __gmp_n) __GMP_NOTHROW
    {
        mp_limb_t  __gmp_c;
        __GMPN_ADD_1 (__gmp_c, __gmp_dst, __gmp_src, __gmp_size, __gmp_n);
        return __gmp_c;
    }
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpn_cmp)
#if ! defined (__GMP_FORCE_mpn_cmp)
    __GMP_EXTERN_INLINE
#endif
        int
        mpn_cmp (mp_srcptr __gmp_xp, mp_srcptr __gmp_yp, mp_size_t __gmp_size) __GMP_NOTHROW
    {
        int __gmp_result;
        __GMPN_CMP (__gmp_result, __gmp_xp, __gmp_yp, __gmp_size);
        return __gmp_result;
    }
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpn_sub)
#if ! defined (__GMP_FORCE_mpn_sub)
    __GMP_EXTERN_INLINE
#endif
        mp_limb_t
        mpn_sub (mp_ptr __gmp_wp, mp_srcptr __gmp_xp, mp_size_t __gmp_xsize, mp_srcptr __gmp_yp, mp_size_t __gmp_ysize)
    {
        mp_limb_t  __gmp_c;
        __GMPN_SUB (__gmp_c, __gmp_wp, __gmp_xp, __gmp_xsize, __gmp_yp, __gmp_ysize);
        return __gmp_c;
    }
#endif

#if defined (__GMP_EXTERN_INLINE) || defined (__GMP_FORCE_mpn_sub_1)
#if ! defined (__GMP_FORCE_mpn_sub_1)
    __GMP_EXTERN_INLINE
#endif
        mp_limb_t
        mpn_sub_1 (mp_ptr __gmp_dst, mp_srcptr __gmp_src, mp_size_t __gmp_size, mp_limb_t __gmp_n) __GMP_NOTHROW
    {
        mp_limb_t  __gmp_c;
        __GMPN_SUB_1 (__gmp_c, __gmp_dst, __gmp_src, __gmp_size, __gmp_n);
        return __gmp_c;
    }
#endif

#if defined (__cplusplus)
}
#endif


/* 允许更快地测试负、零和正 */
#define mpz_sgn(Z) ((Z)->_mp_size < 0 ? -1 : (Z)->_mp_size > 0)
#define mpf_sgn(F) ((F)->_mp_size < 0 ? -1 : (F)->_mp_size > 0)
#define mpq_sgn(Q) ((Q)->_mp_num._mp_size < 0 ? -1 : (Q)->_mp_num._mp_size > 0)

/* 使用GCC时，优化某些常见比较 */
#if defined (__GNUC__)
#define mpz_cmp_ui(Z,UI) \
(__builtin_constant_p (UI) && (UI) == 0                \
     ? mpz_sgn (Z) : _mpz_cmp_ui (Z,UI))
#define mpz_cmp_si(Z,SI) \
    (__builtin_constant_p (SI) && (SI) == 0 ? mpz_sgn (Z)            \
     : __builtin_constant_p (SI) && (SI) > 0                \
         ? _mpz_cmp_ui (Z, __GMP_CAST (unsigned long int, SI))        \
         : _mpz_cmp_si (Z,SI))
#define mpq_cmp_ui(Q,NUI,DUI) \
    (__builtin_constant_p (NUI) && (NUI) == 0                \
         ? mpq_sgn (Q) : _mpq_cmp_ui (Q,NUI,DUI))
#define mpq_cmp_si(q,n,d)                       \
    (__builtin_constant_p ((n) >= 0) && (n) >= 0  \
         ? mpq_cmp_ui (q, __GMP_CAST (unsigned long, n), d) \
         : _mpq_cmp_si (q, n, d))
#else
#define mpz_cmp_ui(Z,UI) _mpz_cmp_ui (Z,UI)
#define mpz_cmp_si(Z,UI) _mpz_cmp_si (Z,UI)
#define mpq_cmp_ui(Q,NUI,DUI) _mpq_cmp_ui (Q,NUI,DUI)
#define mpq_cmp_si(q,n,d)  _mpq_cmp_si(q,n,d)
#endif


/* 使用"&"而不是"&&"意味着这些可以无分支地出来。每个
   mpz_t至少分配了一个limb，因此获取低位limb始终是允许的 */
#define mpz_odd_p(z)   (((z)->_mp_size != 0) & __GMP_CAST (int, (z)->_mp_d[0]))
#define mpz_even_p(z)  (! mpz_odd_p (z))


/**************** C++例程 ****************/

#ifdef __cplusplus
    __GMP_DECLSPEC_XX std::ostream& operator<< (std::ostream &, mpz_srcptr);
__GMP_DECLSPEC_XX std::ostream& operator<< (std::ostream &, mpq_srcptr);
__GMP_DECLSPEC_XX std::ostream& operator<< (std::ostream &, mpf_srcptr);
__GMP_DECLSPEC_XX std::istream& operator>> (std::istream &, mpz_ptr);
__GMP_DECLSPEC_XX std::istream& operator>> (std::istream &, mpq_ptr);
__GMP_DECLSPEC_XX std::istream& operator>> (std::istream &, mpf_ptr);
#endif


/* 与GMP 2及更早版本的源码级兼容性 */
#define mpn_divmod(qp,np,nsize,dp,dsize) \
mpn_divrem (qp, __GMP_CAST (mp_size_t, 0), np, nsize, dp, dsize)

/* 与GMP 1的源码级兼容性 */
#define mpz_mdiv    mpz_fdiv_q
#define mpz_mdivmod    mpz_fdiv_qr
#define mpz_mmod    mpz_fdiv_r
#define mpz_mdiv_ui    mpz_fdiv_q_ui
#define mpz_mdivmod_ui(q,r,n,d) \
    (((r) == 0) ? mpz_fdiv_q_ui (q,n,d) : mpz_fdiv_qr_ui (q,r,n,d))
#define mpz_mmod_ui(r,n,d) \
    (((r) == 0) ? mpz_fdiv_ui (n,d) : mpz_fdiv_r_ui (r,n,d))

/* 有用的同义词，但与GMP 1不完全兼容 */
#define mpz_div        mpz_fdiv_q
#define mpz_divmod    mpz_fdiv_qr
#define mpz_div_ui    mpz_fdiv_q_ui
#define mpz_divmod_ui    mpz_fdiv_qr_ui
#define mpz_div_2exp    mpz_fdiv_q_2exp
#define mpz_mod_2exp    mpz_fdiv_r_2exp

    enum
{
    GMP_ERROR_NONE = 0,
    GMP_ERROR_UNSUPPORTED_ARGUMENT = 1,
    GMP_ERROR_DIVISION_BY_ZERO = 2,
    GMP_ERROR_SQRT_OF_NEGATIVE = 4,
    GMP_ERROR_INVALID_ARGUMENT = 8
};

/* 主版本号也是__GNU_MP__的值，上面和mp.h中 */
#define __GNU_MP_VERSION 4
#define __GNU_MP_VERSION_MINOR 2
#define __GNU_MP_VERSION_PATCHLEVEL 2

#define __GMP_H__
#endif /* __GMP_H__ */
