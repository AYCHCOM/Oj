/* oj.c
 * Copyright (c) 2012, Peter Ohler
 *
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * 
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * 
 * - Neither the name of Peter Ohler nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "oj.h"

// maximum to allocate on the stack, arbitrary limit
#define SMALL_JSON	65536

typedef struct _YesNoOpt {
    VALUE       sym;
    char        *attr;
} *YesNoOpt;

void Init_oj();

VALUE    Oj = Qnil;

ID	oj_as_json_id;
ID	oj_at_id;
ID	oj_fileno_id;
ID	oj_instance_variables_id;
ID	oj_json_create_id;
ID	oj_read_id;
ID	oj_string_id;
ID	oj_to_hash_id;
ID	oj_to_json_id;
ID	oj_to_sym_id;
ID	oj_tv_nsec_id;
ID	oj_tv_sec_id;
ID	oj_tv_usec_id;
ID	oj_write_id;

VALUE	oj_bag_class;
VALUE	oj_date_class;
VALUE	oj_stringio_class;
VALUE	oj_struct_class;
VALUE	oj_time_class;

VALUE	oj_slash_string;

static VALUE	ascii_only_sym;
static VALUE	auto_define_sym;
static VALUE	circular_sym;
static VALUE	compat_sym;
static VALUE	encoding_sym;
static VALUE	indent_sym;
static VALUE	mode_sym;
static VALUE	null_sym;
static VALUE	object_sym;
static VALUE	strict_sym;
static VALUE	symbol_keys_sym;

static VALUE	array_nl_sym;
static VALUE	create_additions_sym;
static VALUE	object_nl_sym;
static VALUE	space_before_sym;
static VALUE	space_sym;
static VALUE	symbolize_names_sym;

static VALUE	mimic = Qnil;
static VALUE	keep = Qnil;

Cache   oj_class_cache = 0;
Cache   oj_attr_cache = 0;

struct _Options	oj_default_options = {
    0,			// encoding
    0,			// indent
    No,			// circular
    Yes,		// auto_define
    No,			// sym_key
    No,			// ascii_only
    ObjectMode,		// mode
    0,			// dump_opts
};

static VALUE	define_mimic_json(VALUE self);

/* call-seq: default_options() => Hash
 *
 * Returns the default load and dump options as a Hash. The options are
 * - indent: [Fixnum] number of spaces to indent each element in an JSON document
 * - encoding: [String|Encoding] character encoding for the JSON coument
 * - circular: [true|false|nil] support circular references while dumping
 * - auto_define: [true|false|nil] automatically define classes if they do not exist
 * - symbol_keys: [true|false|nil] use symbols instead of strings for hash keys
 * - mode: [:object|:strict|:compat|:null] load and dump modes to use for JSON
 * @return [Hash] all current option settings.
 */
static VALUE
get_def_opts(VALUE self) {
    VALUE       opts = rb_hash_new();
    
#ifdef HAVE_RUBY_ENCODING_H
    rb_hash_aset(opts, encoding_sym, (0 == oj_default_options.encoding) ? Qnil : rb_enc_from_encoding(oj_default_options.encoding));
#endif
    rb_hash_aset(opts, indent_sym, INT2FIX(oj_default_options.indent));
    rb_hash_aset(opts, circular_sym, (Yes == oj_default_options.circular) ? Qtrue : ((No == oj_default_options.circular) ? Qfalse : Qnil));
    rb_hash_aset(opts, auto_define_sym, (Yes == oj_default_options.auto_define) ? Qtrue : ((No == oj_default_options.auto_define) ? Qfalse : Qnil));
    rb_hash_aset(opts, ascii_only_sym, (Yes == oj_default_options.ascii_only) ? Qtrue : ((No == oj_default_options.ascii_only) ? Qfalse : Qnil));
    rb_hash_aset(opts, symbol_keys_sym, (Yes == oj_default_options.sym_key) ? Qtrue : ((No == oj_default_options.sym_key) ? Qfalse : Qnil));
    switch (oj_default_options.mode) {
    case StrictMode:	rb_hash_aset(opts, mode_sym, strict_sym);	break;
    case CompatMode:	rb_hash_aset(opts, mode_sym, compat_sym);	break;
    case NullMode:	rb_hash_aset(opts, mode_sym, null_sym);		break;
    case ObjectMode:
    default:            rb_hash_aset(opts, mode_sym, object_sym);	break;
    }
    return opts;
}

/* call-seq: default_options=(opts)
 *
 * Sets the default options for load and dump.
 * @param [Hash] opts options to change
 * @param [Fixnum] :indent number of spaces to indent each element in an JSON document
 * @param [String] :encoding character encoding for the JSON file
 * @param [true|false|nil] :circular support circular references while dumping
 * @param [true|false|nil] :auto_define automatically define classes if they do not exist
 * @param [true|false|nil] :symbol_keys convert hash keys to symbols
 * @param [true|false|nil] :ascii_only encode all high-bit characters as escaped sequences if true
 * @param [:object|:strict|:compat|:null] load and dump mode to use for JSON
 *        :strict raises an exception when a non-supported Object is
 *        encountered. :compat attempts to extract variable values from an
 *        Object using to_json() or to_hash() then it walks the Object's
 *        variables if neither is found. The :object mode ignores to_hash()
 *        and to_json() methods and encodes variables using code internal to
 *        the Oj gem. The :null mode ignores non-supported Objects and
 *        replaces them with a null.  @return [nil]
 */
static VALUE
set_def_opts(VALUE self, VALUE opts) {
    struct _YesNoOpt    ynos[] = {
        { circular_sym, &oj_default_options.circular },
        { auto_define_sym, &oj_default_options.auto_define },
        { symbol_keys_sym, &oj_default_options.sym_key },
        { ascii_only_sym, &oj_default_options.ascii_only },
        { Qnil, 0 }
    };
    YesNoOpt    o;
    VALUE       v;
    
    Check_Type(opts, T_HASH);

#ifdef HAVE_RUBY_ENCODING_H
    if (Qtrue == rb_funcall(opts, rb_intern("has_key?"), 1, encoding_sym)) {
	v = rb_hash_lookup(opts, encoding_sym);
	if (Qnil == v) {
	    oj_default_options.encoding = 0;
	} else if (T_STRING == rb_type(v)) {
	    oj_default_options.encoding = rb_enc_find(StringValuePtr(v));
	} else if (rb_cEncoding == rb_obj_class(v)) {
	    oj_default_options.encoding = rb_to_encoding(v);
	} else {
	    rb_raise(rb_eArgError, ":encoding must be nil, a String, or an Encoding.\n");
	}
    }
#endif
    v = rb_hash_aref(opts, indent_sym);
    if (Qnil != v) {
        Check_Type(v, T_FIXNUM);
        oj_default_options.indent = FIX2INT(v);
    }

    v = rb_hash_lookup(opts, mode_sym);
    if (Qnil == v) {
	// ignore
    } else if (object_sym == v) {
        oj_default_options.mode = ObjectMode;
    } else if (strict_sym == v) {
        oj_default_options.mode = StrictMode;
    } else if (compat_sym == v) {
        oj_default_options.mode = CompatMode;
    } else if (null_sym == v) {
        oj_default_options.mode = NullMode;
    } else {
        rb_raise(rb_eArgError, ":mode must be :object, :strict, :compat, or :null.\n");
    }

    for (o = ynos; 0 != o->attr; o++) {
	if (Qtrue != rb_funcall(opts, rb_intern("has_key?"), 1, o->sym)) {
	    continue;
	}
        if (Qnil != (v = rb_hash_lookup(opts, o->sym))) {
	    if (Qtrue == v) {
		*o->attr = Yes;
	    } else if (Qfalse == v) {
		*o->attr = No;
	    } else {
		rb_raise(rb_eArgError, "%s must be true, false, or nil.\n", rb_id2name(SYM2ID(o->sym)));
	    }
	}
    }
    return Qnil;
}

static void
parse_options(VALUE ropts, Options copts) {
    struct _YesNoOpt    ynos[] = {
        { circular_sym, &copts->circular },
        { auto_define_sym, &copts->auto_define },
        { symbol_keys_sym, &copts->sym_key },
        { ascii_only_sym, &copts->ascii_only },
        { Qnil, 0 }
    };
    YesNoOpt    o;
    
    if (rb_cHash == rb_obj_class(ropts)) {
        VALUE   v;
        
        if (Qnil != (v = rb_hash_lookup(ropts, indent_sym))) {
            if (rb_cFixnum != rb_obj_class(v)) {
                rb_raise(rb_eArgError, ":indent must be a Fixnum.\n");
            }
            copts->indent = NUM2INT(v);
        }
#ifdef HAVE_RUBY_ENCODING_H
        if (Qnil != (v = rb_hash_lookup(ropts, encoding_sym))) {
	    if (T_STRING == rb_type(v)) {
		oj_default_options.encoding = rb_enc_find(StringValuePtr(v));
	    } else if (rb_cEncoding == rb_obj_class(v)) {
		oj_default_options.encoding = rb_to_encoding(v);
	    } else {
		rb_raise(rb_eArgError, ":encoding must be nil, a String, or an Encoding.\n");
	    }
        }
#endif
        if (Qnil != (v = rb_hash_lookup(ropts, mode_sym))) {
            if (object_sym == v) {
                copts->mode = ObjectMode;
            } else if (strict_sym == v) {
                copts->mode = StrictMode;
            } else if (compat_sym == v) {
                copts->mode = CompatMode;
            } else if (null_sym == v) {
                copts->mode = NullMode;
            } else {
                rb_raise(rb_eArgError, ":mode must be :object, :strict, :compat, or :null.\n");
            }
        }
        for (o = ynos; 0 != o->attr; o++) {
            if (Qnil != (v = rb_hash_lookup(ropts, o->sym))) {
                if (Qtrue == v) {
                    *o->attr = Yes;
                } else if (Qfalse == v) {
                    *o->attr = No;
                } else {
                    rb_raise(rb_eArgError, "%s must be true or false.\n", rb_id2name(SYM2ID(o->sym)));
                }
            }
        }
    }
 }

static VALUE
load_with_opts(VALUE input, Options copts) {
    char        *json;
    size_t	len;
    VALUE	obj;

    if (rb_type(input) == T_STRING) {
	// the json string gets modified so make a copy of it
	len = RSTRING_LEN(input) + 1;
	if (SMALL_JSON < len) {
	    json = ALLOC_N(char, len);
	} else {
	    json = ALLOCA_N(char, len);
	}
	strcpy(json, StringValuePtr(input));
    } else {
	VALUE	clas = rb_obj_class(input);
	VALUE	s;

	if (oj_stringio_class == clas) {
	    s = rb_funcall2(input, oj_string_id, 0, 0);
	    len = RSTRING_LEN(s) + 1;
	    if (SMALL_JSON < len) {
		json = ALLOC_N(char, len);
	    } else {
		json = ALLOCA_N(char, len);
	    }
	    strcpy(json, StringValuePtr(s));
	} else if (rb_respond_to(input, oj_fileno_id) && Qnil != (s = rb_funcall(input, oj_fileno_id, 0))) {
	    int		fd = FIX2INT(s);
	    ssize_t	cnt;

	    len = lseek(fd, 0, SEEK_END);
	    lseek(fd, 0, SEEK_SET);
	    if (SMALL_JSON < len) {
		json = ALLOC_N(char, len + 1);
	    } else {
		json = ALLOCA_N(char, len + 1);
	    }
	    if (0 >= (cnt = read(fd, json, len)) || cnt != (ssize_t)len) {
		rb_raise(rb_eIOError, "failed to read from IO Object.\n");
	    }
	    json[len] = '\0';
	} else if (rb_respond_to(input, oj_read_id)) {
	    s = rb_funcall2(input, oj_read_id, 0, 0);
	    len = RSTRING_LEN(s) + 1;
	    if (SMALL_JSON < len) {
		json = ALLOC_N(char, len);
	    } else {
		json = ALLOCA_N(char, len);
	    }
	    strcpy(json, StringValuePtr(s));
	} else {
	    rb_raise(rb_eArgError, "load() expected a String or IO Object.\n");
	}
    }
    obj = oj_parse(json, copts);
    if (SMALL_JSON < len) {
	xfree(json);
    }
    return obj;
}

/* call-seq: load(json, options) => Hash, Array, String, Fixnum, Float, true, false, or nil
 *
 * Parses a JSON document String into a Hash, Array, String, Fixnum, Float,
 * true, false, or nil. Raises an exception if the JSON is malformed or the
 * classes specified are not valid.
 * @param [String] json JSON String
 * @param [Hash] options load options (same as default_options)
 */
static VALUE
load(int argc, VALUE *argv, VALUE self) {
    struct _Options	options = oj_default_options;

    if (1 > argc) {
	rb_raise(rb_eArgError, "Wrong number of arguments to load().\n");
    }
    if (2 <= argc) {
	parse_options(argv[1], &options);
    }
    return load_with_opts(*argv, &options);
}

static VALUE
load_file(int argc, VALUE *argv, VALUE self) {
    char                *path;
    char                *json;
    FILE                *f;
    unsigned long       len;
    VALUE		obj;
    struct _Options	options = oj_default_options;

    Check_Type(*argv, T_STRING);
    path = StringValuePtr(*argv);
    if (0 == (f = fopen(path, "r"))) {
        rb_raise(rb_eIOError, "%s\n", strerror(errno));
    }
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    if (SMALL_JSON < len) {
	json = ALLOC_N(char, len + 1);
    } else {
	json = ALLOCA_N(char, len + 1);
    }
    fseek(f, 0, SEEK_SET);
    if (len != fread(json, 1, len, f)) {
        fclose(f);
        rb_raise(rb_eLoadError, "Failed to read %ld bytes from %s.\n", len, path);
    }
    fclose(f);
    json[len] = '\0';
    if (2 <= argc) {
	parse_options(argv[1], &options);
    }
    obj = oj_parse(json, &options);
    if (SMALL_JSON < len) {
	xfree(json);
    }
    return obj;
}

/* call-seq: dump(obj, options) => json-string
 *
 * Dumps an Object (obj) to a string.
 * @param [Object] obj Object to serialize as an JSON document String
 * @param [Hash] options same as default_options
 */
static VALUE
dump(int argc, VALUE *argv, VALUE self) {
    char                *json;
    struct _Options     copts = oj_default_options;
    VALUE               rstr;
    
    if (2 == argc) {
        parse_options(argv[1], &copts);
    }
    if (0 == (json = oj_write_obj_to_str(*argv, &copts))) {
        rb_raise(rb_eNoMemError, "Not enough memory.\n");
    }
    rstr = rb_str_new2(json);
#ifdef HAVE_RUBY_ENCODING_H
    if (0 != copts.encoding) {
        rb_enc_associate(rstr, copts.encoding);
    }
#endif
    xfree(json);

    return rstr;
}


/* call-seq: to_file(file_path, obj, options)
 *
 * Dumps an Object to the specified file.
 * @param [String] file_path file path to write the JSON document to
 * @param [Object] obj Object to serialize as an JSON document String
 * @param [Hash] options formating options
 * @param [Fixnum] :indent format expected
 * @param [true|false] :circular allow circular references, default: false
 */
static VALUE
to_file(int argc, VALUE *argv, VALUE self) {
    struct _Options     copts = oj_default_options;
    
    if (3 == argc) {
        parse_options(argv[2], &copts);
    }
    Check_Type(*argv, T_STRING);
    oj_write_obj_to_file(argv[1], StringValuePtr(*argv), &copts);

    return Qnil;
}

// Mimic JSON section

static VALUE
mimic_dump(int argc, VALUE *argv, VALUE self) {
    char                *json;
    struct _Options     copts = oj_default_options;
    VALUE               rstr;
    
    if (0 == (json = oj_write_obj_to_str(*argv, &copts))) {
        rb_raise(rb_eNoMemError, "Not enough memory.\n");
    }
    rstr = rb_str_new2(json);
#ifdef ENCODING_INLINE_MAX
    if (0 != copts.encoding) {
	rb_enc_associate(rstr, copts.encoding);
    }
#endif
    if (2 <= argc && Qnil != argv[1]) {
	VALUE	io = argv[1];
	VALUE	args[1];

	*args = rstr;
	rb_funcall2(io, oj_write_id, 1, args);
	rstr = io;
    }
    xfree(json);

    return rstr;
}

// This is the signature for the hash_foreach callback also.
static int
mimic_walk(VALUE key, VALUE obj, VALUE proc) {
    switch (rb_type(obj)) {
    case T_HASH:
	rb_hash_foreach(obj, mimic_walk, proc);
	break;
    case T_ARRAY:
	{
	    VALUE	*np = RARRAY_PTR(obj);
	    size_t	cnt = RARRAY_LEN(obj);
	
	    for (; 0 < cnt; cnt--, np++) {
		mimic_walk(Qnil, *np, proc);
	    }
	    break;
	}
    default:
	break;
    }
    if (Qnil == proc) {
	if (rb_block_given_p()) {
	    rb_yield(obj);
	}
    } else {
#ifdef RUBINIUS
        rb_raise(rb_eNotImpError, "Not supported in Rubinius.\n");
#else
	VALUE	args[1];

	*args = obj;
	rb_proc_call_with_block(proc, 1, args, Qnil);
#endif
    }
    return ST_CONTINUE;
}

static VALUE
mimic_load(int argc, VALUE *argv, VALUE self) {
    VALUE	obj = load(1, argv, self);
    VALUE	p = Qnil;

    if (2 <= argc) {
	p = argv[1];
    }
    mimic_walk(Qnil, obj, p);

    return obj;
}

static VALUE
mimic_dump_load(int argc, VALUE *argv, VALUE self) {
    if (1 > argc) {
        rb_raise(rb_eArgError, "wrong number of arguments (0 for 1)\n");
    } else if (T_STRING == rb_type(*argv)) {
	return mimic_load(argc, argv, self);
    } else {
	return mimic_dump(argc, argv, self);
    }
}

static VALUE
mimic_generate_core(int argc, VALUE *argv, Options copts) {
    char                *json;
    VALUE               rstr;
    
    if (2 == argc && Qnil != argv[1]) {
	struct _DumpOpts	dump_opts;
	VALUE			ropts = argv[1];
        VALUE   		v;

	memset(&dump_opts, 0, sizeof(dump_opts)); // may not be needed
	if (T_HASH != rb_type(ropts)) {
	    rb_raise(rb_eArgError, "options must be a hash.\n");
	}
        if (Qnil != (v = rb_hash_lookup(ropts, indent_sym))) {
	    rb_check_type(v, T_STRING);
	    if (0 == copts->dump_opts) {
		copts->dump_opts = &dump_opts;
	    }
	    copts->dump_opts->indent = StringValuePtr(v);
	    copts->dump_opts->indent_size = (uint8_t)strlen(copts->dump_opts->indent);
	}
        if (Qnil != (v = rb_hash_lookup(ropts, space_sym))) {
	    rb_check_type(v, T_STRING);
	    if (0 == copts->dump_opts) {
		copts->dump_opts = &dump_opts;
	    }
	    copts->dump_opts->after_sep = StringValuePtr(v);
	    copts->dump_opts->after_size = (uint8_t)strlen(copts->dump_opts->after_sep);
	}
        if (Qnil != (v = rb_hash_lookup(ropts, space_before_sym))) {
	    rb_check_type(v, T_STRING);
	    if (0 == copts->dump_opts) {
		copts->dump_opts = &dump_opts;
	    }
	    copts->dump_opts->before_sep = StringValuePtr(v);
	    copts->dump_opts->before_size = (uint8_t)strlen(copts->dump_opts->before_sep);
	}
        if (Qnil != (v = rb_hash_lookup(ropts, object_nl_sym))) {
	    rb_check_type(v, T_STRING);
	    if (0 == copts->dump_opts) {
		copts->dump_opts = &dump_opts;
	    }
	    copts->dump_opts->hash_nl = StringValuePtr(v);
	    copts->dump_opts->hash_size = (uint8_t)strlen(copts->dump_opts->hash_nl);
	}
        if (Qnil != (v = rb_hash_lookup(ropts, array_nl_sym))) {
	    rb_check_type(v, T_STRING);
	    if (0 == copts->dump_opts) {
		copts->dump_opts = &dump_opts;
	    }
	    copts->dump_opts->array_nl = StringValuePtr(v);
	    copts->dump_opts->array_size = (uint8_t)strlen(copts->dump_opts->array_nl);
	}
	// :allow_nan is not supported as Oj always allows_nan
	// :max_nesting is always set to 100
    }
    if (0 == (json = oj_write_obj_to_str(*argv, copts))) {
        rb_raise(rb_eNoMemError, "Not enough memory.\n");
    }
    rstr = rb_str_new2(json);
#ifdef ENCODING_INLINE_MAX
    if (0 != copts->encoding) {
        rb_enc_associate(rstr, copts->encoding);
    }
#endif
    xfree(json);

    return rstr;
}

static VALUE
mimic_generate(int argc, VALUE *argv, VALUE self) {
    struct _Options     copts = oj_default_options;

    return mimic_generate_core(argc, argv, &copts);
}

static VALUE
mimic_pretty_generate(int argc, VALUE *argv, VALUE self) {
    struct _Options     copts = oj_default_options;
    struct _DumpOpts	dump_opts;
    
    dump_opts.indent = "  ";
    dump_opts.indent_size = (uint8_t)strlen(dump_opts.indent);
    dump_opts.before_sep = " ";
    dump_opts.before_size = (uint8_t)strlen(dump_opts.before_sep);
    dump_opts.after_sep = " ";
    dump_opts.after_size = (uint8_t)strlen(dump_opts.after_sep);
    dump_opts.hash_nl = "\n";
    dump_opts.hash_size = (uint8_t)strlen(dump_opts.hash_nl);
    dump_opts.array_nl = "\n";
    dump_opts.array_size = (uint8_t)strlen(dump_opts.array_nl);
    copts.dump_opts = &dump_opts;

    return mimic_generate_core(argc, argv, &copts);
}

static VALUE
mimic_parse(int argc, VALUE *argv, VALUE self) {
    struct _Options	options = oj_default_options;

    if (1 > argc) {
	rb_raise(rb_eArgError, "Wrong number of arguments to load().\n");
    }
    if (2 <= argc && Qnil != argv[1]) {
	VALUE	ropts = argv[1];
        VALUE   v;

	if (T_HASH != rb_type(ropts)) {
	    rb_raise(rb_eArgError, "options must be a hash.\n");
	}
        if (Qnil != (v = rb_hash_lookup(ropts, symbolize_names_sym))) {
	    options.sym_key = (Qtrue == v) ? Yes : No;
	}
        if (Qnil != (v = rb_hash_lookup(ropts,  create_additions_sym))) {
	    options.mode = (Qtrue == v) ? CompatMode : StrictMode;
	}
	// :allow_nan is not supported as Oj always allows_nan
	// :max_nesting is always set to 100
	// :object_class is always Hash
	// :array_class is always Array
    }
    return load_with_opts(*argv, &options);
}

static VALUE
mimic_recurse_proc(VALUE self, VALUE obj) {
    rb_need_block();
    mimic_walk(Qnil, obj, Qnil);

    return Qnil;
}

static VALUE
no_op1(VALUE self, VALUE obj) {
    return Qnil;
}

/* call-seq: mimic_JSON() => Module
 *
 * Creates the JSON module with methods and classes to mimic the JSON
 * gem. After this method is invoked calls that expect the JSON module will
 * use Oj instead and be faster than the original JSON. Most options that
 * could be passed to the JSON methods are supported. The calls to set parser
 * or generator will not raise an Exception but will not have any effect.
 */
static VALUE
define_mimic_json(VALUE self) {
    if (Qnil == mimic) {
	VALUE	ext;

	mimic = rb_define_module("JSON");
	ext = rb_define_module_under(mimic, "Ext");
	rb_define_class_under(ext, "Parser", rb_cObject);
	rb_define_class_under(ext, "Generator", rb_cObject);

	rb_define_module_function(mimic, "parser=", no_op1, 1);
	rb_define_module_function(mimic, "generator=", no_op1, 1);

	rb_define_module_function(mimic, "dump", mimic_dump, -1);
	rb_define_module_function(mimic, "load", mimic_load, -1);
	rb_define_module_function(mimic, "restore", mimic_load, -1);
	rb_define_module_function(mimic, "recurse_proc", mimic_recurse_proc, 1);
	rb_define_module_function(mimic, "[]", mimic_dump_load, -1);
	rb_define_module_function(mimic, "generate", mimic_generate, -1);
	rb_define_module_function(mimic, "fast_generate", mimic_generate, -1);
	rb_define_module_function(mimic, "pretty_generate", mimic_pretty_generate, -1);
	rb_define_module_function(mimic, "parse", mimic_parse, -1);
	rb_define_module_function(mimic, "parse!", mimic_parse, -1);

	array_nl_sym = ID2SYM(rb_intern("array_nl"));	rb_ary_push(keep, array_nl_sym);
	create_additions_sym = ID2SYM(rb_intern("create_additions"));	rb_ary_push(keep, create_additions_sym);
	object_nl_sym = ID2SYM(rb_intern("object_nl"));	rb_ary_push(keep, object_nl_sym);
	space_before_sym = ID2SYM(rb_intern("space_before"));	rb_ary_push(keep, space_before_sym);
	space_sym = ID2SYM(rb_intern("space"));	rb_ary_push(keep, space_sym);
	symbolize_names_sym = ID2SYM(rb_intern("symbolize_names"));	rb_ary_push(keep, symbolize_names_sym);

	oj_default_options.mode = CompatMode;
	oj_default_options.ascii_only = Yes;
    }
    return mimic;
}

void Init_oj() {
    Oj = rb_define_module("Oj");
    keep = rb_cv_get(Oj, "@@keep"); // needed to stop GC from deleting and reusing VALUEs

    rb_require("time");
    rb_require("date");
    rb_require("stringio");

    rb_define_module_function(Oj, "default_options", get_def_opts, 0);
    rb_define_module_function(Oj, "default_options=", set_def_opts, 1);

    rb_define_module_function(Oj, "mimic_JSON", define_mimic_json, 0);
    rb_define_module_function(Oj, "load", load, -1);
    rb_define_module_function(Oj, "load_file", load_file, -1);
    rb_define_module_function(Oj, "dump", dump, -1);
    rb_define_module_function(Oj, "to_file", to_file, -1);

    oj_as_json_id = rb_intern("as_json");
    oj_at_id = rb_intern("at");
    oj_fileno_id = rb_intern("fileno");
    oj_instance_variables_id = rb_intern("instance_variables");
    oj_json_create_id = rb_intern("json_create");
    oj_read_id = rb_intern("read");
    oj_string_id = rb_intern("string");
    oj_to_hash_id = rb_intern("to_hash");
    oj_to_json_id = rb_intern("to_json");
    oj_to_sym_id = rb_intern("to_sym");
    oj_tv_nsec_id = rb_intern("tv_nsec");
    oj_tv_sec_id = rb_intern("tv_sec");
    oj_tv_usec_id = rb_intern("tv_usec");
    oj_write_id = rb_intern("write");

    oj_bag_class = rb_const_get_at(Oj, rb_intern("Bag"));
    oj_struct_class = rb_const_get(rb_cObject, rb_intern("Struct"));
    oj_time_class = rb_const_get(rb_cObject, rb_intern("Time"));
    oj_date_class = rb_const_get(rb_cObject, rb_intern("Date"));
    oj_stringio_class = rb_const_get(rb_cObject, rb_intern("StringIO"));

    ascii_only_sym = ID2SYM(rb_intern("ascii_only"));	rb_ary_push(keep, ascii_only_sym);
    auto_define_sym = ID2SYM(rb_intern("auto_define"));	rb_ary_push(keep, auto_define_sym);
    circular_sym = ID2SYM(rb_intern("circular"));	rb_ary_push(keep, circular_sym);
    compat_sym = ID2SYM(rb_intern("compat"));		rb_ary_push(keep, compat_sym);
    encoding_sym = ID2SYM(rb_intern("encoding"));	rb_ary_push(keep, encoding_sym);
    indent_sym = ID2SYM(rb_intern("indent"));		rb_ary_push(keep, indent_sym);
    mode_sym = ID2SYM(rb_intern("mode"));		rb_ary_push(keep, mode_sym);
    symbol_keys_sym = ID2SYM(rb_intern("symbol_keys"));	rb_ary_push(keep, symbol_keys_sym);
    null_sym = ID2SYM(rb_intern("null"));		rb_ary_push(keep, null_sym);
    object_sym = ID2SYM(rb_intern("object"));		rb_ary_push(keep, object_sym);
    strict_sym = ID2SYM(rb_intern("strict"));		rb_ary_push(keep, strict_sym);

    oj_slash_string = rb_str_new2("/");			rb_ary_push(keep, oj_slash_string);

    oj_default_options.mode = ObjectMode;
#ifdef HAVE_RUBY_ENCODING_H
    oj_default_options.encoding = rb_enc_find("UTF-8");
#endif

    oj_cache_new(&oj_class_cache);
    oj_cache_new(&oj_attr_cache);

    oj_init_doc();
}

void
_oj_raise_error(const char *msg, const char *xml, const char *current, const char* file, int line) {
    int         xline = 1;
    int         col = 1;

    for (; xml < current && '\n' != *current; current--) {
        col++;
    }
    for (; xml < current; current--) {
        if ('\n' == *current) {
            xline++;
        }
    }
    rb_raise(rb_eSyntaxError, "%s at line %d, column %d [%s:%d]\n", msg, xline, col, file, line);
}
