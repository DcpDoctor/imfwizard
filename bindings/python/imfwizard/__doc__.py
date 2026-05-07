"""IMF Wizard Python Bindings

Python scripting interface for creating SMPTE ST 2067 IMF packages.
Built with SWIG from the C++ library.

Example:
    >>> import imfwizard
    >>> opts = imfwizard.ImpOptions()
    >>> opts.title = "My Feature"
    >>> opts.video_dir = "/path/to/j2k_frames"
    >>> opts.audio_file = "/path/to/audio.wav"
    >>> opts.output_dir = "/path/to/output"
    >>> result = imfwizard.create_ov_imp(opts)
    >>> print(result.cpl_uuid)
"""
