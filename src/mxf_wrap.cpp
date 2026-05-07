#include "imfwizard/mxf_wrap.h"
#include "imfwizard/uuid.h"
#include "imfwizard/hash.h"
#include <KM_fileio.h>
#include <KM_prng.h>
#include <AS_02.h>
#include <PCMParserList.h>
#include <Metadata.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <stdexcept>

namespace imfwizard {

static const ASDCP::Dictionary* get_dict()
{
    const ASDCP::Dictionary* dict = &ASDCP::DefaultSMPTEDict();
    return dict;
}

static MxfTrackFile wrap_j2k(const WrapOptions& opts)
{
    using namespace ASDCP;

    const Dictionary* dict = get_dict();
    AS_02::JP2K::MXFWriter writer;
    JP2K::SequenceParser parser;
    JP2K::FrameBuffer frame_buffer(4 * Kumu::Megabyte);

    Rational edit_rate(opts.edit_rate_num, opts.edit_rate_den);

    Result_t result = parser.OpenRead(opts.input_dir.string());
    if (ASDCP_FAILURE(result))
        throw std::runtime_error("Failed to open J2K sequence: " + opts.input_dir.string());

    JP2K::PictureDescriptor pdesc;
    parser.FillPictureDescriptor(pdesc);
    pdesc.EditRate = edit_rate;

    spdlog::info("J2K sequence: {}x{}, {} frames",
                 pdesc.StoredWidth, pdesc.StoredHeight, pdesc.ContainerDuration);

    MXF::FileDescriptor* essence_descriptor = new MXF::RGBAEssenceDescriptor(dict);
    MXF::InterchangeObject_list_t essence_sub_descriptors;

    ASDCP::MXF::RGBAEssenceDescriptor* rgba_desc =
        dynamic_cast<ASDCP::MXF::RGBAEssenceDescriptor*>(essence_descriptor);
    rgba_desc->SampleRate = edit_rate;
    rgba_desc->ContainerDuration = pdesc.ContainerDuration;
    rgba_desc->StoredWidth = pdesc.StoredWidth;
    rgba_desc->StoredHeight = pdesc.StoredHeight;
    rgba_desc->AspectRatio = Rational(pdesc.StoredWidth, pdesc.StoredHeight);

    ASDCP::MXF::JPEG2000PictureSubDescriptor* j2k_sub =
        new ASDCP::MXF::JPEG2000PictureSubDescriptor(dict);
    essence_sub_descriptors.push_back(j2k_sub);

    WriterInfo info;
    info.CompanyName = "IMF Wizard";
    info.ProductName = "IMF Wizard";
    info.ProductVersion = "0.1.0";
    Kumu::GenRandomUUID(info.AssetUUID);

    result = writer.OpenWrite(opts.output_path.string(), info, essence_descriptor,
                              essence_sub_descriptors, edit_rate, 16384,
                              AS_02::IS_FOLLOW, 10);
    if (ASDCP_FAILURE(result))
        throw std::runtime_error("Failed to open MXF for writing: " + opts.output_path.string());

    uint32_t frame_count = 0;
    while (ASDCP_SUCCESS(parser.ReadFrame(frame_buffer))) {
        result = writer.WriteFrame(frame_buffer, nullptr, nullptr);
        if (ASDCP_FAILURE(result))
            throw std::runtime_error("Failed to write frame " + std::to_string(frame_count));
        ++frame_count;
    }

    result = writer.Finalize();
    if (ASDCP_FAILURE(result))
        throw std::runtime_error("Failed to finalize MXF");

    spdlog::info("Wrapped {} J2K frames -> {}", frame_count, opts.output_path.string());

    MxfTrackFile track;
    track.path = opts.output_path;
    Kumu::UUID asset_uuid(info.AssetUUID);
    char uuid_buf[64];
    asset_uuid.EncodeString(uuid_buf, sizeof(uuid_buf));
    track.uuid = uuid_buf;
    track.duration = frame_count;
    track.hash = sha1_base64(opts.output_path);
    track.size = std::filesystem::file_size(opts.output_path);

    return track;
}

static MxfTrackFile wrap_pcm(const WrapOptions& opts)
{
    using namespace ASDCP;

    const Dictionary* dict = get_dict();
    AS_02::PCM::MXFWriter writer;
    PCM::FrameBuffer frame_buffer;

    Rational edit_rate(opts.edit_rate_num, opts.edit_rate_den);

    PCM::AudioDescriptor adesc;
    adesc.EditRate = edit_rate;
    adesc.AudioSamplingRate = Rational(opts.sample_rate, 1);
    adesc.QuantizationBits = opts.bit_depth;
    adesc.BlockAlign = (opts.bit_depth / 8) * opts.channels;
    adesc.ChannelCount = opts.channels;
    adesc.AvgBps = opts.sample_rate * adesc.BlockAlign;

    // Read WAV header to get actual params
    PCMParserList parser;
    std::list<std::string> file_list;
    file_list.push_back(opts.input_dir.string());

    Result_t result = parser.OpenRead(file_list, edit_rate);
    if (ASDCP_FAILURE(result))
        throw std::runtime_error("Failed to open WAV: " + opts.input_dir.string());

    parser.FillAudioDescriptor(adesc);

    MXF::WaveAudioDescriptor* wave_desc = new MXF::WaveAudioDescriptor(dict);
    wave_desc->SampleRate = edit_rate;
    wave_desc->AudioSamplingRate = adesc.AudioSamplingRate;
    wave_desc->QuantizationBits = adesc.QuantizationBits;
    wave_desc->ChannelCount = adesc.ChannelCount;
    wave_desc->BlockAlign = adesc.BlockAlign;
    wave_desc->AvgBps = adesc.AvgBps;
    wave_desc->ContainerDuration = adesc.ContainerDuration;

    MXF::InterchangeObject_list_t essence_sub_descriptors;

    uint32_t samples_per_frame = AS_02::MXF::CalcSamplesPerFrame(*wave_desc, edit_rate);
    frame_buffer.Capacity(AS_02::MXF::CalcFrameBufferSize(*wave_desc, edit_rate));

    WriterInfo info;
    info.CompanyName = "IMF Wizard";
    info.ProductName = "IMF Wizard";
    info.ProductVersion = "0.1.0";
    Kumu::GenRandomUUID(info.AssetUUID);

    result = writer.OpenWrite(opts.output_path.string(), info,
                              wave_desc, essence_sub_descriptors,
                              edit_rate, 16384);
    if (ASDCP_FAILURE(result))
        throw std::runtime_error("Failed to open audio MXF for writing");

    uint32_t frame_count = 0;
    while (ASDCP_SUCCESS(parser.ReadFrame(frame_buffer))) {
        result = writer.WriteFrame(frame_buffer, nullptr, nullptr);
        if (ASDCP_FAILURE(result))
            throw std::runtime_error("Failed to write audio frame " + std::to_string(frame_count));
        ++frame_count;
    }

    result = writer.Finalize();
    if (ASDCP_FAILURE(result))
        throw std::runtime_error("Failed to finalize audio MXF");

    spdlog::info("Wrapped {} audio frames -> {}", frame_count, opts.output_path.string());

    MxfTrackFile track;
    track.path = opts.output_path;
    Kumu::UUID asset_uuid_a(info.AssetUUID);
    char uuid_buf[64];
    asset_uuid_a.EncodeString(uuid_buf, sizeof(uuid_buf));
    track.uuid = uuid_buf;
    track.duration = frame_count;
    track.hash = sha1_base64(opts.output_path);
    track.size = std::filesystem::file_size(opts.output_path);

    return track;
}

MxfTrackFile wrap_essence(const WrapOptions& opts)
{
    switch (opts.type) {
    case EssenceType::J2K:
        return wrap_j2k(opts);
    case EssenceType::WAV:
        return wrap_pcm(opts);
    default:
        throw std::runtime_error("Unsupported essence type");
    }
}

} // namespace imfwizard
