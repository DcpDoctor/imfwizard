#include "imfwizard/pkl.h"
#include "imfwizard/uuid.h"
#include "imfwizard/hash.h"
#include <libxml/tree.h>
#include <spdlog/spdlog.h>
#include <ctime>
#include <stdexcept>

namespace imfwizard {

static const char* PKL_NS = "http://www.smpte-ra.org/schemas/429-8/2007/PKL";

PklResult generate_pkl(const PklOptions& opts,
                       const std::filesystem::path& output_dir)
{
    std::string pkl_uuid = generate_uuid_bare();
    std::filesystem::path pkl_path = output_dir / ("PKL_" + pkl_uuid + ".xml");

    xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
    xmlNodePtr root = xmlNewNode(nullptr, BAD_CAST "PackingList");
    xmlDocSetRootElement(doc, root);
    xmlNewNs(root, BAD_CAST PKL_NS, nullptr);

    xmlNewChild(root, nullptr, BAD_CAST "Id", BAD_CAST ("urn:uuid:" + pkl_uuid).c_str());
    xmlNewChild(root, nullptr, BAD_CAST "Issuer", BAD_CAST opts.issuer.c_str());

    // IssueDate in ISO 8601
    time_t now = time(nullptr);
    struct tm tm_buf;
    gmtime_r(&now, &tm_buf);
    char date_buf[64];
    strftime(date_buf, sizeof(date_buf), "%Y-%m-%dT%H:%M:%S+00:00", &tm_buf);
    xmlNewChild(root, nullptr, BAD_CAST "IssueDate", BAD_CAST date_buf);

    // AssetList
    xmlNodePtr asset_list = xmlNewChild(root, nullptr, BAD_CAST "AssetList", nullptr);
    for (const auto& asset : opts.assets) {
        xmlNodePtr node = xmlNewChild(asset_list, nullptr, BAD_CAST "Asset", nullptr);
        xmlNewChild(node, nullptr, BAD_CAST "Id", BAD_CAST ("urn:uuid:" + asset.uuid).c_str());
        xmlNewChild(node, nullptr, BAD_CAST "Hash", BAD_CAST asset.hash.c_str());
        xmlNewChild(node, nullptr, BAD_CAST "Size", BAD_CAST std::to_string(asset.size).c_str());
        xmlNewChild(node, nullptr, BAD_CAST "Type", BAD_CAST asset.type.c_str());
        xmlNewChild(node, nullptr, BAD_CAST "OriginalFileName",
                    BAD_CAST asset.original_filename.c_str());
    }

    xmlSaveFormatFileEnc(pkl_path.string().c_str(), doc, "UTF-8", 1);
    xmlFreeDoc(doc);

    PklResult result;
    result.uuid = pkl_uuid;
    result.path = pkl_path;
    result.hash = sha1_base64(pkl_path);
    result.size = std::filesystem::file_size(pkl_path);

    spdlog::info("Generated PKL: {}", pkl_path.string());
    return result;
}

} // namespace imfwizard
