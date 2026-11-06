/*************************************************************************
*  2004      Olivier Galizzi                                             *
*  2004-2021 Janek Kozicki                                               *
*                                                                        *
*  This program is free software; it is licensed under the terms of the  *
*  GNU General Public License v2 or later. See file LICENSE for details. *
*************************************************************************/

#pragma once

#include <boost/preprocessor/arithmetic/dec.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/comparison/not_equal.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/seq/size.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/tuple/elem.hpp>
#include <boost/python.hpp>
#include <boost/type_traits/integral_constant.hpp>
#include <type_traits>

#include <lib/base/AliasNamespaces.hpp>
#include <lib/base/Math.hpp>
#include <lib/factory/Factorable.hpp>
#include <lib/pyutil/doc_opts.hpp>
#include <lib/pyutil/raw_constructor.hpp>
// Include it as the last one: for high precision types, make sure that def_readonly(…) does not return internal reference.
#include <lib/serialization/PyClassCustom.hpp>

namespace yade {

template <typename T> void preLoad(T&) { };
template <typename T> void postLoad(T&) { };
template <typename T> void preSave(T&) { };
template <typename T> void postSave(T&) { };


// attribute flags

namespace Attr {
	// keep in sync with py/wrapper/yadeWrapper.cpp !
	enum flags { noSave = 1, readonly = 2, triggerPostLoad = 4, hidden = 8, noResize = 16 };
};


// see:
// (old site, fixed bug) https://bugs.launchpad.net/yade/+bug/539562
// http://www.boost.org/doc/libs/1_42_0/libs/python/doc/v2/faq.html#topythonconversionfailed
// for reason why the original def_readwrite will not work:
// #define _PYATTR_DEF(x,thisClass,z) .def_readwrite(BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(2,0,z)),&thisClass::BOOST_PP_TUPLE_ELEM(2,0,z),BOOST_PP_TUPLE_ELEM(2,1,z))
#define _PYATTR_DEF(x, thisClass, z) _DEF_READWRITE_CUSTOM(thisClass, z)
//
// return reference for vector and matrix types to allow things like
// O.bodies.pos[1].state.vel[2]=0
// returning value would only change copy of velocity, without propagating back to the original
//
// see http://www.mail-archive.com/yade-dev@lists.launchpad.net/msg03406.html
//
// note that for sequences (like vector<> etc), values are returned; but in case of
// vector of shared_ptr's, things inside are still shared, so
// O.engines[2].gravity=(0,0,9.33) will work
//
// OTOH got sequences of non-shared types, it sill (silently) fail:
// f=Facet(); f.vertices[1][0]=4
//
// see http://www.boost.org/doc/libs/1_42_0/libs/type_traits/doc/html/boost_typetraits/background.html
// about how this works

// by default, do not return reference; return value instead
template <typename T> struct py_wrap_ref : public boost::false_type { };
// specialize for types that should be returned as references
template <> struct py_wrap_ref<Vector3r> : public boost::true_type { };
template <> struct py_wrap_ref<Vector3i> : public boost::true_type { };
template <> struct py_wrap_ref<Vector2r> : public boost::true_type { };
template <> struct py_wrap_ref<Vector2i> : public boost::true_type { };
template <> struct py_wrap_ref<Quaternionr> : public boost::true_type { };
template <> struct py_wrap_ref<Matrix3r> : public boost::true_type { };

// ADL only works within the same namespace
// this duplicate is for classes that are not in yade:: namespace (yet)
template <class C, typename T, T C::* A> void make_setter_postLoad(C& instance, const T& val)
{
	instance.*A = val;
	instance.callPostLoad();
}


#define _DEF_READWRITE_BY_VALUE(thisClass, attr, doc)                                                                                                          \
	add_property(/*attr name*/ BOOST_PP_STRINGIZE(attr),                                                                                                                                         \
	        /*read access*/ ::boost::python::make_getter(&thisClass::attr, ::boost::python::return_value_policy<::boost::python::return_by_value>()),      \
	        /*write access*/ ::boost::python::make_setter(&thisClass::attr, ::boost::python::return_value_policy<::boost::python::return_by_value>()),     \
	        /*docstring*/ doc)

// not sure if this is correct: the getter works by value, the setter by reference (the default)...?
#define _DEF_READWRITE_BY_VALUE_POSTLOAD(thisClass, attr, doc)                                                                                                 \
	add_property(/*attr name*/ BOOST_PP_STRINGIZE(attr),                                                                                                                                         \
	        /*read access*/ ::boost::python::make_getter(&thisClass::attr, ::boost::python::return_value_policy<::boost::python::return_by_value>()),      \
	        /*write access*/ make_setter_postLoad<thisClass, decltype(thisClass::attr), &thisClass::attr>,                                                 \
	        /*docstring*/ doc)

#define _DEF_READONLY_BY_VALUE(thisClass, attr, doc)                                                                                                           \
	add_property(/*attr name*/ BOOST_PP_STRINGIZE(attr),                                                                                                                                         \
	        /*read access*/ ::boost::python::make_getter(&thisClass::attr, ::boost::python::return_value_policy<::boost::python::return_by_value>()),      \
	        /*docstring*/ doc)

/* Huh, add_static_property does not support doc argument (add_property does); if so, use add_property for now at least... */
#define _DEF_READWRITE_BY_VALUE_STATIC(thisClass, attr, doc) _DEF_READWRITE_BY_VALUE(thisClass, attr, doc)

template <int Flags, class C, typename T, T C::* Member, class PythonClass>
void registerSerializableAttribute(PythonClass& classObj, const char* name, const char* doc, const char* className)
{
	if constexpr (!(Flags & Attr::hidden)) {
		constexpr bool readonly = Flags & Attr::readonly, postload = Flags & Attr::triggerPostLoad;
		std::string    docStr(doc);
		docStr += " :yattrflags:`" + boost::lexical_cast<std::string>(Flags) + "` ";
		if constexpr (!py_wrap_ref<T>::value) {
			auto getter = ::boost::python::make_getter(Member, ::boost::python::return_value_policy<::boost::python::return_by_value>());
			if constexpr (readonly) classObj.add_property(name, getter, docStr.c_str());
			else if constexpr (postload)
				classObj.add_property(name, getter, make_setter_postLoad<C, T, Member>, docStr.c_str());
			else
				classObj.add_property(
				        name,
				        getter,
				        ::boost::python::make_setter(Member, ::boost::python::return_value_policy<::boost::python::return_by_value>()),
				        docStr.c_str());
		} else if constexpr (readonly)
			classObj.def_readonly(name, Member, docStr.c_str());
		else if constexpr (postload)
			classObj.add_property(name, ::boost::python::make_getter(Member), make_setter_postLoad<C, T, Member>, docStr.c_str());
		else
			classObj.def_readwrite(name, Member, docStr.c_str());
		if constexpr (readonly && postload)
			std::cerr << "WARN: " << className << "::" << name
			          << " with the yade::Attr::readonly flag also uselessly sets yade::Attr::triggerPostLoad." << std::endl;
	}
}

#define _DEF_READWRITE_CUSTOM(thisClass, attr)                                                                                                                 \
	yade::registerSerializableAttribute<_ATTR_FLG(attr), thisClass, decltype(thisClass::_ATTR_NAM(attr)), &thisClass::_ATTR_NAM(attr)>(                    \
	        _classObj, _ATTR_NAM_STR(attr), _ATTR_DOC(attr), BOOST_PP_STRINGIZE(thisClass));
#define _DEF_READWRITE_CUSTOM_STATIC(thisClass, attr, doc)                                                                                                     \
	{                                                                                                                                                      \
		_classObj.def_readwrite(BOOST_PP_STRINGIZE(attr), &thisClass::attr, doc);                                                                      \
	}


#if (YADE_REAL_BIT <= 80)
#define _PYSET_ATTR_DEPREC(x, thisClass, z)                                                                                                                    \
	if (key == BOOST_PP_STRINGIZE(_DEPREC_OLDNAME(z))) {                                                                                                   \
		_DEPREC_WARN(thisClass, z);                                                                                                                    \
		_DEPREC_NEWNAME(z) = ::boost::python::extract<decltype(_DEPREC_NEWNAME(z))>(value);                                                            \
		return;                                                                                                                                        \
	}
#else
#define _PYSET_ATTR_DEPREC(x, thisClass, z)                                                                                                                    \
	if (key == BOOST_PP_STRINGIZE(_DEPREC_OLDNAME(z))) {                                                                                                   \
		_DEPREC_WARN(thisClass, z);                                                                                                                    \
		_DEPREC_NEWNAME(z) = static_cast<decltype(_DEPREC_NEWNAME(z))>(::boost::python::extract<decltype(_DEPREC_NEWNAME(z))>(value));                 \
		return;                                                                                                                                        \
	}
#endif

#define _PYATTR_DEPREC_DEF(x, thisClass, z)                                                                                                                    \
	.add_property(                                                                                                                                         \
	        BOOST_PP_STRINGIZE(_DEPREC_OLDNAME(z)),                                                                                                        \
	                           &thisClass::BOOST_PP_CAT(_getDeprec_, _DEPREC_OLDNAME(z)),                                                                  \
	                           &thisClass::BOOST_PP_CAT(_setDeprec_, _DEPREC_OLDNAME(z)),                                                                  \
	                           "|ydeprecated| alias for :yref:`" BOOST_PP_STRINGIZE(_DEPREC_NEWNAME(z)) "<" BOOST_PP_STRINGIZE(thisClass) "." BOOST_PP_STRINGIZE(             \
	                _DEPREC_NEWNAME(z)) ">` (" _DEPREC_COMMENT(z) ")")
#define _PYHASKEY_ATTR_DEPREC(x, thisClass, z)                                                                                                                 \
	if (key == BOOST_PP_STRINGIZE(_DEPREC_OLDNAME(z))) return true;

/* accessors functions ussing warning */
#define _ACCESS_DEPREC(x, thisClass, z) /*getter*/                                                                                                             \
	decltype(_DEPREC_NEWNAME(z)) BOOST_PP_CAT(_getDeprec_, _DEPREC_OLDNAME(z))()                                                                           \
	{                                                                                                                                                      \
		_DEPREC_WARN(thisClass, z);                                                                                                                    \
		return _DEPREC_NEWNAME(z);                                                                                                                     \
	} /*setter*/                                                                                                                                           \
	void BOOST_PP_CAT(_setDeprec_, _DEPREC_OLDNAME(z))(const decltype(_DEPREC_NEWNAME(z))& val)                                                            \
	{                                                                                                                                                      \
		_DEPREC_WARN(thisClass, z);                                                                                                                    \
		_DEPREC_NEWNAME(z) = val;                                                                                                                      \
	}

// loop bodies for attribute access
#define _PYGET_ATTR(x, y, z)                                                                                                                                   \
	if (key == _ATTR_NAM_STR(z)) return ::boost::python::object(_ATTR_NAM(z));
#if (YADE_REAL_BIT <= 80)
#define _PYSET_ATTR(x, y, z)                                                                                                                                   \
	if (key == _ATTR_NAM_STR(z)) {                                                                                                                         \
		_ATTR_NAM(z) = ::boost::python::extract<decltype(_ATTR_NAM(z))>(value);                                                                        \
		return;                                                                                                                                        \
	}
#else
#define _PYSET_ATTR(x, y, z)                                                                                                                                   \
	if (key == _ATTR_NAM_STR(z)) {                                                                                                                         \
		_ATTR_NAM(z) = static_cast<decltype(_ATTR_NAM(z))>(::boost::python::extract<decltype(_ATTR_NAM(z))>(value));                                   \
		return;                                                                                                                                        \
	}
#endif
#define _PYKEYS_ATTR(x, y, z) ret.append(_ATTR_NAM_STR(z));
#define _PYHASKEY_ATTR(x, y, z)                                                                                                                                \
	if (key == _ATTR_NAM_STR(z)) return true;
template <int Flags, class T> void addSerializableDictAttribute(::boost::python::dict& dictionary, const char* name, const T& value)
{
	if constexpr (!(Flags & Attr::hidden)) dictionary[name] = ::boost::python::object(value);
}
#define _PYDICT_ATTR(x, y, z) addSerializableDictAttribute<_ATTR_FLG(z)>(ret, _ATTR_NAM_STR(z), _ATTR_NAM(z));
#define _REGISTER_BOOST_ATTRIBUTES_REPEAT(x, y, z)                                                                                                             \
	if constexpr ((_ATTR_FLG(z) & yade::Attr::noSave) == 0) {                                                                                              \
		ar& BOOST_SERIALIZATION_NVP(_ATTR_NAM(z));                                                                                                     \
	}
#define _REGISTER_BOOST_ATTRIBUTES(baseClass, attrs)                                                                                                           \
	friend class boost::serialization::access;                                                                                                             \
                                                                                                                                                               \
private:                                                                                                                                                       \
	template <class ArchiveT> void serialize(ArchiveT& ar, unsigned int /*version*/)                                                                       \
	{                                                                                                                                                      \
		ar& BOOST_SERIALIZATION_BASE_OBJECT_NVP(baseClass);                                                                                            \
		/* with ADL, either the generic (empty) version above or baseClass::preLoad etc will be called (compile-time resolution) */                    \
		if (ArchiveT::is_loading::value) preLoad(*this);                                                                                               \
		else                                                                                                                                           \
			preSave(*this);                                                                                                                        \
		BOOST_PP_SEQ_FOR_EACH(_REGISTER_BOOST_ATTRIBUTES_REPEAT, ~, attrs)                                                                             \
		if (ArchiveT::is_loading::value) postLoad(*this);                                                                                              \
		else                                                                                                                                           \
			postSave(*this);                                                                                                                       \
	}

#define _REGISTER_ATTRIBUTES_DEPREC(thisClass, baseClass, attrs, deprec)                                                                                                                                                                                                   \
_REGISTER_BOOST_ATTRIBUTES(baseClass, attrs) public : void pySetAttr(const std::string& key, const ::boost::python::object& value) override                                                                                                                                \
	{                                                                                                                                                                                                                                                                  \
		BOOST_PP_SEQ_FOR_EACH(_PYSET_ATTR, ~, attrs);                                                                                                                                                                                                              \
		BOOST_PP_SEQ_FOR_EACH(_PYSET_ATTR_DEPREC, thisClass, deprec);                                                                                                                                                                                              \
		baseClass::pySetAttr(key, value);                                                                                                                                                                                                                          \
	}                                                                                                                                                                                                                                                                  \
	/* list all attributes (except deprecated ones); could return ::boost::python::set instead*/ /* ::boost::python::list pyKeys() const {  ::boost::python::list ret; BOOST_PP_SEQ_FOR_EACH(_PYKEYS_ATTR,~,attrs); ret.extend(baseClass::pyKeys()); return ret; }  */ \
	/* return dictionary of all acttributes and values; deprecated attributes omitted */ ::boost::python::dict pyDict() const override                                                                                                                                 \
	{                                                                                                                                                                                                                                                                  \
		::boost::python::dict ret;                                                                                                                                                                                                                                 \
		BOOST_PP_SEQ_FOR_EACH(_PYDICT_ATTR, ~, attrs);                                                                                                                                                                                                             \
		ret.update(this->pyDictCustom());                                                                                                                                                                                                                          \
		ret.update(baseClass::pyDict());                                                                                                                                                                                                                           \
		return ret;                                                                                                                                                                                                                                                \
	}                                                                                                                                                                                                                                                                  \
	void callPostLoad(void) override                                                                                                                                                                                                                                   \
	{                                                                                                                                                                                                                                                                  \
		baseClass::callPostLoad();                                                                                                                                                                                                                                 \
		postLoad(*this);                                                                                                                                                                                                                                           \
	}


// print warning about deprecated attribute; thisClass is type name, not string
#define _DEPREC_WARN(thisClass, deprec)                                                                                                                        \
	std::cerr << "WARN: " << getClassName() << "."                                                                                                         \
	          << BOOST_PP_STRINGIZE(_DEPREC_OLDNAME(deprec))                                                                                                                        \
	                                << " is deprecated, use "                                                                                              \
	                                << BOOST_PP_STRINGIZE(thisClass) << "." << BOOST_PP_STRINGIZE(_DEPREC_NEWNAME(deprec)) << " instead. ";                \
	if (_DEPREC_COMMENT(deprec)) {                                                                                                                         \
		if (std::string(_DEPREC_COMMENT(deprec))[0] == '!') {                                                                                          \
			std::cerr << endl;                                                                                                                     \
			throw std::invalid_argument(                                                                                                           \
			        BOOST_PP_STRINGIZE(thisClass) "." BOOST_PP_STRINGIZE(                                                      \
			        _DEPREC_OLDNAME(deprec)) " is deprecated; throwing exception requested. Reason: " _DEPREC_COMMENT(deprec));                            \
		} else                                                                                                                                         \
			std::cerr << "(" << _DEPREC_COMMENT(deprec) << ")";                                                                                    \
	}                                                                                                                                                      \
	std::cerr << std::endl;

// getters for individual fields
#define _ATTR_TYP(s) BOOST_PP_TUPLE_ELEM(5, 0, s)
#define _ATTR_NAM(s) BOOST_PP_TUPLE_ELEM(5, 1, s)
#define _ATTR_INI(s) BOOST_PP_TUPLE_ELEM(5, 2, s)
#define _ATTR_FLG(s)                                                                                                                                           \
	(BOOST_PP_TUPLE_ELEM(5, 3, s)                                                                                                                          \
	 + 0) // ( ) to avoid operator precedence mess, +0 expands either to unary + or addition; we need () around the arg, but it must be correct if arg is empty as well
#define _ATTR_DOC(s) BOOST_PP_TUPLE_ELEM(5, 4, s)
// stringized getters
#define _ATTR_TYP_STR(s) BOOST_PP_STRINGIZE(_ATTR_TYP(s))
#define _ATTR_NAM_STR(s) BOOST_PP_STRINGIZE(_ATTR_NAM(s))
#define _ATTR_INI_STR(s) BOOST_PP_STRINGIZE(_ATTR_INI(s))
// embed default and type values in the docstring (must keep the same size and ordering)
#define _ATTRS_EMBED_INI_TYP_IN_DOC(x, y, z)                                                                                                                   \
	((_ATTR_TYP(z),                                                                                                                                        \
	  _ATTR_NAM(z),                                                                                                                                        \
	  _ATTR_INI(z),                                                                                                                                        \
	  _ATTR_FLG(z),                                                                                                                                        \
	  _ATTR_DOC(z) " :ydefault:`" _ATTR_INI_STR(z) "`"                                                                                                     \
	                                               " :yattrtype:`" _ATTR_TYP_STR(z) "`"))

// deprecated specification getters
#define _DEPREC_OLDNAME(x) BOOST_PP_TUPLE_ELEM(3, 0, x)
#define _DEPREC_NEWNAME(x) BOOST_PP_TUPLE_ELEM(3, 1, x)
#define _DEPREC_COMMENT(x) BOOST_PP_TUPLE_ELEM(3, 2, x) "" // if the argument is omited, return empty string instead of nothing


// Bindings are instantiated only by the class's owning plugin, not by header consumers.
template <class...> using SerializableBindingVoid = void;
template <class C, class = void> struct SerializableHasOwnBindings : std::false_type { };
template <class C>
struct SerializableHasOwnBindings<C, SerializableBindingVoid<typename C::YadePyRegistrationOwner>> : std::is_same<C, typename C::YadePyRegistrationOwner> { };

template <class C>
typename std::enable_if<SerializableHasOwnBindings<C>::value>::type registerSerializableClassImplementation(C& instance, const ::boost::python::object& scope)
{
	instance.template pyRegisterClassImpl<C>(scope);
}
template <class C>
typename std::enable_if<!SerializableHasOwnBindings<C>::value>::type registerSerializableClassImplementation(C&, const ::boost::python::object&)
{
	// Handwritten classes retain their existing virtual registration implementation.
}

#define _YADE_DECLARE_PYTHON_BINDINGS(thisClass)                                                                                                               \
	using YadePyRegistrationOwner = thisClass;                                                                                                             \
	void pyRegisterClass(::boost::python::object scope) override { registerSerializableClassImplementation(*this, scope); }                                \
	template <class YadeBindingClass = thisClass> void pyRegisterClassImpl(const ::boost::python::object& _scope)

#define DEFINE_SERIALIZABLE_BINDINGS(name)                                                                                                                     \
	template void registerSerializableClassImplementation<name>(name&, const ::boost::python::object&);                                                    \
	DEFINE_SERIALIZABLE_ARCHIVES(name)

#undef _YADE_PLUGIN_PYTHON_REGISTER
#define _YADE_PLUGIN_PYTHON_REGISTER(x, y, z) DEFINE_SERIALIZABLE_BINDINGS(z)


#define _YADE_CLASS_BASE_DOC_ATTRS_DEPREC_PY(thisClass, baseClass, docString, attrs, deprec, extras)                                                           \
	_REGISTER_ATTRIBUTES_DEPREC(thisClass, baseClass, attrs, deprec)                                                                                       \
	REGISTER_CLASS_AND_BASE(thisClass, baseClass)                                                                                                          \
	/* accessors for deprecated attributes, with warnings */ BOOST_PP_SEQ_FOR_EACH(                                                                        \
	        _ACCESS_DEPREC, thisClass, deprec) /* python class registration */ _YADE_DECLARE_PYTHON_BINDINGS(thisClass)                                    \
	{                                                                                                                                                      \
		checkPyClassRegistersItself(#thisClass);                                                                                                       \
		::boost::python::scope thisScope(_scope);                                                                                                      \
		YADE_SET_DOCSTRING_OPTS;                                                                                                                       \
		::boost::python::PyClassCustom<YadeBindingClass, shared_ptr<YadeBindingClass>, ::boost::python::bases<baseClass>, boost::noncopyable>          \
		        _classObj(#thisClass, docString);                                                                                                      \
		_classObj.def("__init__", ::boost::python::raw_constructor(Serializable_ctor_kwAttrs<YadeBindingClass>));                                      \
		BOOST_PP_SEQ_FOR_EACH(_PYATTR_DEF, thisClass, attrs);                                                                                          \
		(void)_classObj BOOST_PP_SEQ_FOR_EACH(_PYATTR_DEPREC_DEF, thisClass, deprec);                                                                  \
		(void)_classObj extras;                                                                                                                        \
	}
// use later: void must_use_both_YADE_CLASS_BASE_DOC_ATTRS_and_YADE_PLUGIN();
// #define YADE_CLASS_BASE_DOC_ATTRS_PY(thisClass,baseClass,docString,attrs,extras) YADE_CLASS_BASE_DOC_ATTRS_DEPREC_PY(thisClass,baseClass,docString,attrs,,extras)

// return "type name;" (for declaration inside class body)
#define _ATTR_DECL(x, y, z) _ATTR_TYP(z) _ATTR_NAM(z);
// return name(default), (for initializers list); TRICKY: last one must not have the comma
#define _ATTR_MAKE_INITIALIZER(x, maxIndex, i, z) BOOST_PP_TUPLE_ELEM(2, 0, z)(BOOST_PP_TUPLE_ELEM(2, 1, z)) BOOST_PP_COMMA_IF(BOOST_PP_NOT_EQUAL(maxIndex, i))
// attrDecl is (type,name,defaultValue,docString)

#define _ATTR_MAKE_INIT_TUPLE(x, y, z) ((_ATTR_NAM(z), _ATTR_INI(z)))


#define _ATTR_NAME_ADD_DUMMY_FIELDS(x, y, z) ((/*type*/, z, /*default*/, /*flags*/, /*doc*/))

#define _STAT_NONSTAT_ATTR_PY(thisClass, attr, doc)                                                                                                            \
	_DEF_READWRITE_CUSTOM_STATIC(thisClass, attr, doc)                                                                                                     \
	/* _DEF_READWRITE_CUSTOM(thisClass,attr,doc) */ /* duplicate static and non-static attributes do not work (they apparently trigger to-python converter being added; for now, make then non-static, that's it. */
#define _STATATTR_PY(x, thisClass, z)                                                                                                                          \
	_STAT_NONSTAT_ATTR_PY(                                                                                                                                 \
	        thisClass, _ATTR_NAM(z), /*docstring*/ "|ystatic| :ydefault:`" _ATTR_INI_STR(z) "` :yattrtype:`" _ATTR_TYP_STR(z) "` " _ATTR_DOC(z))
#define _STATATTR_DECL(x, y, z) static _ATTR_TYP(z) _ATTR_NAM(z);
#define _STATATTR_INITIALIZE(x, thisClass, z) thisClass::_ATTR_NAM(z) = _ATTR_INI(z);
#define _STATATTR_MAKE_DOC(x, thisClass, z)                                                                                                                    \
	".. ystaticattr:: " BOOST_PP_STRINGIZE(thisClass) "." _ATTR_NAM_STR(z) "(=" _ATTR_INI_STR(z) ")"                                                       \
	                                                                                             "\n\n\t" _ATTR_DOC(z) "\n\n"


#define _STATCLASS_PY_REGISTER_CLASS(thisClass, baseClass, docString, attrs)                                                                                   \
	_YADE_DECLARE_PYTHON_BINDINGS(thisClass)                                                                                                               \
	{                                                                                                                                                      \
		checkPyClassRegistersItself(#thisClass);                                                                                                       \
		initSetStaticAttributesValue();                                                                                                                \
		::boost::python::scope thisScope(_scope);                                                                                                      \
		YADE_SET_DOCSTRING_OPTS;                                                                                                                       \
		::boost::python::PyClassCustom<YadeBindingClass, shared_ptr<YadeBindingClass>, ::boost::python::bases<baseClass>, boost::noncopyable>          \
		        _classObj(#thisClass, docString "\n\n" BOOST_PP_SEQ_FOR_EACH(_STATATTR_MAKE_DOC, thisClass, attrs));                                   \
		_classObj.def("__init__", ::boost::python::raw_constructor(Serializable_ctor_kwAttrs<YadeBindingClass>));                                      \
		BOOST_PP_SEQ_FOR_EACH(_STATATTR_PY, thisClass, attrs);                                                                                         \
	}

#define _YADE_CLASS_PYCLASS_BASE_DOC_ATTRS_DEPREC_PY(thisClass, pyClassName, baseClass, docString, attrs, deprec, extras)                                      \
	_REGISTER_ATTRIBUTES_DEPREC(thisClass, baseClass, attrs, deprec)                                                                                       \
	REGISTER_CLASS_AND_BASE(pyClassName, baseClass)                                                                                                        \
	/* accessors for deprecated attributes, with warnings */ BOOST_PP_SEQ_FOR_EACH(                                                                        \
	        _ACCESS_DEPREC, thisClass, deprec) /* python class registration */ _YADE_DECLARE_PYTHON_BINDINGS(thisClass)                                    \
	{                                                                                                                                                      \
		checkPyClassRegistersItself(#pyClassName);                                                                                                     \
		::boost::python::scope thisScope(_scope);                                                                                                      \
		YADE_SET_DOCSTRING_OPTS;                                                                                                                       \
		::boost::python::PyClassCustom<YadeBindingClass, shared_ptr<YadeBindingClass>, ::boost::python::bases<baseClass>, boost::noncopyable>          \
		        _classObj(#pyClassName, docString);                                                                                                    \
		_classObj.def("__init__", ::boost::python::raw_constructor(Serializable_ctor_kwAttrs<YadeBindingClass>));                                      \
		BOOST_PP_SEQ_FOR_EACH(_PYATTR_DEF, thisClass, attrs);                                                                                          \
		(void)_classObj BOOST_PP_SEQ_FOR_EACH(_PYATTR_DEPREC_DEF, thisClass, deprec);                                                                  \
		(void)_classObj extras;                                                                                                                        \
	}


/********************** USER MACROS START HERE ********************/

// attrs is (type,name,init-value,docstring)
#define YADE_CLASS_BASE_DOC(klass, base, doc) YADE_CLASS_BASE_DOC_ATTRS_INIT_CTOR_PY(klass, base, doc, , , , )
#define YADE_CLASS_BASE_DOC_ATTRS(klass, base, doc, attrs) YADE_CLASS_BASE_DOC_ATTRS_INIT_CTOR_PY(klass, base, doc, attrs, , , )
#define YADE_CLASS_BASE_DOC_ATTRS_CTOR(klass, base, doc, attrs, ctor) YADE_CLASS_BASE_DOC_ATTRS_INIT_CTOR_PY(klass, base, doc, attrs, , ctor, )
#define YADE_CLASS_BASE_DOC_ATTRS_CTOR_PY(klass, base, doc, attrs, ctor, py) YADE_CLASS_BASE_DOC_ATTRS_INIT_CTOR_PY(klass, base, doc, attrs, , ctor, py)
#define YADE_CLASS_BASE_DOC_ATTRS_INIT_CTOR_PY(klass, base, doc, attrs, inits, ctor, py)                                                                       \
	YADE_CLASS_BASE_DOC_ATTRS_DEPREC_INIT_CTOR_PY(klass, base, doc, attrs, , inits, ctor, py)

// the most general
#define YADE_CLASS_BASE_DOC_ATTRS_DEPREC_INIT_CTOR_PY(thisClass, baseClass, docString, attrDecls, deprec, inits, ctor, extras)                                 \
public:                                                                                                                                                        \
	BOOST_PP_SEQ_FOR_EACH(_ATTR_DECL, ~, attrDecls) /* attribute declarations */                                                                           \
	thisClass() BOOST_PP_IF(BOOST_PP_SEQ_SIZE(inits attrDecls), :, ) BOOST_PP_SEQ_FOR_EACH_I(                                                              \
	        _ATTR_MAKE_INITIALIZER, BOOST_PP_DEC(BOOST_PP_SEQ_SIZE(inits attrDecls)), inits BOOST_PP_SEQ_FOR_EACH(_ATTR_MAKE_INIT_TUPLE, ~, attrDecls))    \
	{                                                                                                                                                      \
		ctor;                                                                                                                                          \
	} /* ctor, with initialization of defaults */                                                                                                          \
	_YADE_CLASS_BASE_DOC_ATTRS_DEPREC_PY(thisClass, baseClass, docString, BOOST_PP_SEQ_FOR_EACH(_ATTRS_EMBED_INI_TYP_IN_DOC, ~, attrDecls), deprec, extras)

// this one lets you give different class names in c++ and python, necessary for compatibility with c++ templates (else all instaces would have the same class name (ex. in FlowEngine.hpp)
#define YADE_CLASS_PYCLASS_BASE_DOC_ATTRS_DEPREC_INIT_CTOR_PY(thisClass, pyClassName, baseClass, docString, attrDecls, deprec, inits, ctor, extras)            \
public:                                                                                                                                                        \
	BOOST_PP_SEQ_FOR_EACH(_ATTR_DECL, ~, attrDecls) /* attribute declarations */                                                                           \
	thisClass() BOOST_PP_IF(BOOST_PP_SEQ_SIZE(inits attrDecls), :, ) BOOST_PP_SEQ_FOR_EACH_I(                                                              \
	        _ATTR_MAKE_INITIALIZER, BOOST_PP_DEC(BOOST_PP_SEQ_SIZE(inits attrDecls)), inits BOOST_PP_SEQ_FOR_EACH(_ATTR_MAKE_INIT_TUPLE, ~, attrDecls))    \
	{                                                                                                                                                      \
		ctor;                                                                                                                                          \
	} /* ctor, with initialization of defaults */                                                                                                          \
	_YADE_CLASS_PYCLASS_BASE_DOC_ATTRS_DEPREC_PY(                                                                                                          \
	        thisClass, pyClassName, baseClass, docString, BOOST_PP_SEQ_FOR_EACH(_ATTRS_EMBED_INI_TYP_IN_DOC, ~, attrDecls), deprec, extras)

// If you get error "expected declaration before ‘}’ token" it means that you forgot put this macro inside yade namespace.
// The BOOST_CLASS_EXPORT_KEY must be outside yade namespace, because it creates its own namespace in the macro:
// https://github.com/boostorg/serialization/blob/develop/include/boost/serialization/export.hpp#L160
// https://github.com/boostorg/serialization/blob/develop/include/boost/serialization/export.hpp#L173
// So to keep things inside single macro, there is closing     ↓      and opening bracket for        yade namespace ↓
#define REGISTER_SERIALIZABLE(name)                                                                                                                            \
	DECLARE_FACTORABLE(name);                                                                                                                              \
	extern template void registerSerializableClassImplementation<name>(name&, const ::boost::python::object&);                                             \
	}                                                                                                                                                      \
	BOOST_CLASS_EXPORT_KEY(yade::name);                                                                                                                    \
	namespace yade {                                                                                                                                       \
		DECLARE_SERIALIZABLE_ARCHIVES(name);

// Archive implementations have the same owning plugin as factories and Python bindings.
#define YADE_SERIALIZABLE_ARCHIVE_INSTANCES(prefix, name)                                                                                                      \
	}                                                                                                                                                      \
	namespace boost {                                                                                                                                      \
		namespace archive {                                                                                                                            \
			namespace detail {                                                                                                                     \
				prefix template class iserializer<binary_iarchive, ::yade::name>;                                                              \
				prefix template class iserializer<xml_iarchive, ::yade::name>;                                                                 \
				prefix template class oserializer<binary_oarchive, ::yade::name>;                                                              \
				prefix template class oserializer<xml_oarchive, ::yade::name>;                                                                 \
			}                                                                                                                                      \
		}                                                                                                                                              \
	}                                                                                                                                                      \
	namespace yade {

#define DECLARE_SERIALIZABLE_ARCHIVES(name) YADE_SERIALIZABLE_ARCHIVE_INSTANCES(extern, name)
#define DEFINE_SERIALIZABLE_ARCHIVES(name) YADE_SERIALIZABLE_ARCHIVE_INSTANCES(, name)

// for static classes (Gl1 functors, for instance)
#define YADE_CLASS_BASE_DOC_STATICATTRS(thisClass, baseClass, docString, attrs)                                                                                \
public:                                                                                                                                                        \
	BOOST_PP_SEQ_FOR_EACH(_STATATTR_DECL, ~, attrs) /* attribute declarations */                                                                           \
	/* no ctor */                                                                                                                                          \
	REGISTER_CLASS_AND_BASE(thisClass, baseClass);                                                                                                         \
	_REGISTER_ATTRIBUTES_DEPREC(thisClass, baseClass, attrs, )                                                                                             \
	/* called only at class registration, to set initial values; storage still has to be alocated in the cpp file! */                                      \
	void initSetStaticAttributesValue(void) { BOOST_PP_SEQ_FOR_EACH(_STATATTR_INITIALIZE, thisClass, attrs); }                                             \
	_STATCLASS_PY_REGISTER_CLASS(thisClass, baseClass, docString, attrs)

// used only in some exceptional cases, might disappear in the future
#define REGISTER_ATTRIBUTES(baseClass, attrs)                                                                                                                  \
	_REGISTER_ATTRIBUTES_DEPREC(_SOME_CLASS, baseClass, BOOST_PP_SEQ_FOR_EACH(_ATTR_NAME_ADD_DUMMY_FIELDS, ~, attrs), )


class Serializable : public Factorable {
public:
	template <class ArchiveT> void serialize(ArchiveT& /*ar*/, unsigned int /*version*/) { };
	// lovely cast members like in eigen :-)
	template <class DerivedT> const DerivedT& cast() const { return *static_cast<DerivedT*>(this); }
	template <class DerivedT> DerivedT&       cast() { return *static_cast<DerivedT*>(this); }

	Serializable() { };
	virtual ~Serializable() { };
	// comparison of strong equality of 2 objects (by their address)
	bool operator==(const Serializable& other) { return this == &other; }
	bool operator!=(const Serializable& other) { return this != &other; }

	void pyUpdateAttrs(const ::boost::python::dict& d);
	//static void pyUpdateAttrs(const shared_ptr<Serializable>&, const ::boost::python::dict& d);

	virtual void pySetAttr(const std::string& key, const ::boost::python::object& /*value*/)
	{
		PyErr_SetString(PyExc_AttributeError, (std::string("No such attribute: ") + key + ".").c_str());
		::boost::python::throw_error_already_set();
	};
	//virtual ::boost::python::list pyKeys() const { return ::boost::python::list(); };
	virtual ::boost::python::dict pyDict() const { return ::boost::python::dict(); }
	virtual ::boost::python::dict pyDictCustom() const { return ::boost::python::dict(); }
	virtual void                  callPostLoad(void) { postLoad(*this); }
	// check whether the class registers itself or whether it calls virtual function of some base class;
	// that means that the class doesn't register itself properly
	virtual void checkPyClassRegistersItself(const std::string& thisClassName) const;
	// perform class registration; overridden in all classes
	virtual void pyRegisterClass(::boost::python::object _scope);
	// perform any manipulation of arbitrary constructor arguments coming from python, manipulating them in-place;
	// the remainder is passed to the Serializable_ctor_kwAttrs of the respective class (note: args must be empty)
	virtual void pyHandleCustomCtorArgs(::boost::python::tuple& /*args*/, ::boost::python::dict& /*kw*/) { return; }

	//! string representation of this object
	std::string pyStr() { return "<" + getClassName() + " instance at " + boost::lexical_cast<string>(this) + ">"; }

	REGISTER_CLASS_NAME(Serializable);
	REGISTER_BASE_CLASS_NAME(Factorable);
};


// helper functions
template <typename T> shared_ptr<T> Serializable_ctor_kwAttrs(::boost::python::tuple& t, ::boost::python::dict& d)
{
	shared_ptr<T> instance;
	instance = shared_ptr<T>(new T);
	instance->pyHandleCustomCtorArgs(t, d); // can change t and d in-place
	if (::boost::python::len(t) > 0)
		throw runtime_error(
		        "Zero (not " + boost::lexical_cast<string>(::boost::python::len(t))
		        + ") non-keyword constructor arguments required [in Serializable_ctor_kwAttrs; Serializable::pyHandleCustomCtorArgs might had changed "
		          "it after your call].");
	if (::boost::python::len(d) > 0) {
		instance->pyUpdateAttrs(d);
		instance->callPostLoad();
	}
	return instance;
}

}; // namespace yade
