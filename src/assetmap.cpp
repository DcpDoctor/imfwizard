#include "imfwizard/assetmap.h"
#include "imfwizard/uuid.h"
#include <libxml/tree.h>
#include <spdlog/spdlog.h>
#include <ctime>
#include <stdexcept>

namespace imfwizard {

static const char* AM_NS = "http://www.smpte-ra.org/schemas/429-9/2007/AM";

void generate_assetmap(const AssetMapOptions& opts,
                       const std::filesystem::path& output_dir)
{
    std::filesystem::path am_path = output_dir / "ASSETMAP.xml";

    xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr root = xmlNewNode(nullptr, BAD_CAST "AssetMap");
    xmlDocSetRootElement(doc, root);
    xmlNewNs(root, BAD_CAST AM_NS, nullptr);

    xmlNewChild(root, nullptr, BAD_CAST "Id", BAD_CAST generate_uuid().c_str());
    xmlNewChild(root, nullptr, BAD_CAST "Issuer", BAD_CAST opts.issuer.c_str());

    // IssueDate
    time_t now = time(nullptr);
    struct tm tm_buf;
    gmtime_r(&now, &tm_buf);
    char date_buf[64];
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%dT%H:%M:%S+00:00", &tm_buf);
    xmlNewChild(root, nullptr, BAD_CAST "IssueDate", BAD_CAST date_buf);

    // AssetList
    xmlNodePtr asset_list = xmlNewChild(root, nullptr, BAD_CAST "AssetList", nullptr);
    for (const auto& entry : opts.assets) {
        xmlNodePtr asset = xmlNewChild(asset_list, nullptr, BAD_CAST "Asset", nullptr);
        xmlNewChild(asset, nullptr, BAD_CAST "Id", BAD_CAST ("urn:uuid:" + entry.uuid).c_str());

        xmlNodePtr chunk_list = xmlNewChild(asset, nullptr, BAD_CAST "ChunkList", nullptr);
        xmlNodePtr chunk = xmlNewChild(chunk_list, nullptr, BAD_CAST "Chunk", nullptr);
        xmlNewChild(chunk, nullptr, BAD_CAST "Path",
                    BAD_CAST entry.path.string().c_str());
        xmlNewChild(chunk, nullptr, BAD_CAST "Length",
                    BAD_CAST std::to_string(entry.size).c_str());
    }

    xmlSaveFormatFileEnc(am_path.string().c_str(), doc, "UTF-8", 1);
    xmlFreeDoc(doc);

    spdlog::info("Generated ASSETMAP: {}", am_path.string());
}

} // namespace imfwizard
