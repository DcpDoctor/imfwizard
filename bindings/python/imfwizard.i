// SWIG interface file for IMF Wizard Python bindings
%module imfwizard

%{
#include "imfwizard/uuid.h"
#include "imfwizard/hash.h"
#include "imfwizard/hdr.h"
#include "imfwizard/mxf_wrap.h"
#include "imfwizard/cpl.h"
#include "imfwizard/pkl.h"
#include "imfwizard/assetmap.h"
#include "imfwizard/imp.h"
#include "imfwizard/encode.h"
#include "imfwizard/transcode.h"
#include "imfwizard/validate.h"
#include "imfwizard/info.h"
#include "imfwizard/loudness.h"
#include "imfwizard/timed_text.h"
#include "imfwizard/profiles.h"
#include "imfwizard/analytics.h"
#include "imfwizard/burnin.h"
#include "imfwizard/dcp_convert.h"
#include "imfwizard/batch_deliver.h"
#include "imfwizard/prores.h"
#include "imfwizard/preview.h"
%}

// Use STL support
%include "std_string.i"
%include "std_vector.i"
%include "stdint.i"

// Map std::filesystem::path to/from Python str
%typemap(in) std::filesystem::path {
  if (!PyUnicode_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expected a string");
    SWIG_fail;
  }
  $1 = std::filesystem::path(PyUnicode_AsUTF8($input));
}
%typemap(in) const std::filesystem::path& (std::filesystem::path temp) {
  if (!PyUnicode_Check($input)) {
    PyErr_SetString(PyExc_TypeError, "Expected a string");
    SWIG_fail;
  }
  temp = std::filesystem::path(PyUnicode_AsUTF8($input));
  $1 = &temp;
}
%typemap(out) std::filesystem::path {
  $result = PyUnicode_FromString($1.string().c_str());
}
%typemap(typecheck, precedence=SWIG_TYPECHECK_STRING) std::filesystem::path, const std::filesystem::path& {
  $1 = PyUnicode_Check($input) ? 1 : 0;
}

// Map std::optional<T> — expose as value or None (for function return types)
%typemap(out) std::optional<imfwizard::SignOptions> {
  if ($1.has_value()) {
    $result = SWIG_NewPointerObj(new imfwizard::SignOptions($1.value()),
                                  $descriptor(imfwizard::SignOptions*), SWIG_POINTER_OWN);
  } else {
    Py_INCREF(Py_None);
    $result = Py_None;
  }
}
%typemap(in) std::optional<imfwizard::SignOptions> {
  if ($input == Py_None) {
    $1 = std::nullopt;
  } else {
    void* argp;
    int res = SWIG_ConvertPtr($input, &argp, $descriptor(imfwizard::SignOptions*), 0);
    if (!SWIG_IsOK(res)) {
      PyErr_SetString(PyExc_TypeError, "Expected SignOptions or None");
      SWIG_fail;
    }
    $1 = *reinterpret_cast<imfwizard::SignOptions*>(argp);
  }
}
%typemap(in) std::optional<imfwizard::MasteringDisplay> {
  if ($input == Py_None) {
    $1 = std::nullopt;
  } else {
    void* argp;
    int res = SWIG_ConvertPtr($input, &argp, $descriptor(imfwizard::MasteringDisplay*), 0);
    if (!SWIG_IsOK(res)) {
      PyErr_SetString(PyExc_TypeError, "Expected MasteringDisplay or None");
      SWIG_fail;
    }
    $1 = *reinterpret_cast<imfwizard::MasteringDisplay*>(argp);
  }
}
%typemap(in) std::optional<imfwizard::ContentLightLevel> {
  if ($input == Py_None) {
    $1 = std::nullopt;
  } else {
    void* argp;
    int res = SWIG_ConvertPtr($input, &argp, $descriptor(imfwizard::ContentLightLevel*), 0);
    if (!SWIG_IsOK(res)) {
      PyErr_SetString(PyExc_TypeError, "Expected ContentLightLevel or None");
      SWIG_fail;
    }
    $1 = *reinterpret_cast<imfwizard::ContentLightLevel*>(argp);
  }
}

// Ignore callback members (not bindable to Python directly)
%ignore imfwizard::TranscodeOptions::on_progress;

// Ignore std::optional members — provide Python accessors via %extend
%ignore imfwizard::ImpOptions::sign;
%ignore imfwizard::HdrMetadata::mastering_display;
%ignore imfwizard::HdrMetadata::content_light;

// Template instantiations for vectors used in the API
%template(MxfTrackFileVector) std::vector<imfwizard::MxfTrackFile>;
%template(PklAssetVector) std::vector<imfwizard::PklAsset>;
%template(AssetMapEntryVector) std::vector<imfwizard::AssetMapEntry>;
%template(ValidationNoteVector) std::vector<imfwizard::ValidationNote>;
%template(ImpTrackInfoVector) std::vector<imfwizard::ImpTrackInfo>;
%template(StringVector) std::vector<std::string>;

// Parse the headers
%include "imfwizard/uuid.h"
%include "imfwizard/hash.h"
%include "imfwizard/hdr.h"
%include "imfwizard/mxf_wrap.h"
%include "imfwizard/cpl.h"
%include "imfwizard/pkl.h"
%include "imfwizard/assetmap.h"
%include "imfwizard/imp.h"
%include "imfwizard/encode.h"
%include "imfwizard/transcode.h"
%include "imfwizard/validate.h"
%include "imfwizard/info.h"
%include "imfwizard/loudness.h"
%include "imfwizard/timed_text.h"
%include "imfwizard/profiles.h"
%include "imfwizard/analytics.h"
%include "imfwizard/burnin.h"
%include "imfwizard/dcp_convert.h"
%include "imfwizard/batch_deliver.h"
%include "imfwizard/prores.h"
%include "imfwizard/preview.h"

// Python-friendly accessors for std::optional members
%extend imfwizard::ImpOptions {
  void set_sign(const imfwizard::SignOptions& s) { $self->sign = s; }
  void clear_sign() { $self->sign = std::nullopt; }
  bool has_sign() { return $self->sign.has_value(); }
}

%extend imfwizard::HdrMetadata {
  void set_mastering_display(const imfwizard::MasteringDisplay& md) { $self->mastering_display = md; }
  void clear_mastering_display() { $self->mastering_display = std::nullopt; }
  bool has_mastering_display() { return $self->mastering_display.has_value(); }

  void set_content_light(const imfwizard::ContentLightLevel& cl) { $self->content_light = cl; }
  void clear_content_light() { $self->content_light = std::nullopt; }
  bool has_content_light() { return $self->content_light.has_value(); }
}
