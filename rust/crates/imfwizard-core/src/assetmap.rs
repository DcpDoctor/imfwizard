use std::io::Write;
use std::path::Path;

/// Write an ASSETMAP.xml file.
pub fn write_assetmap(path: &Path, pkl_uuid: &str, cpl_uuid: &str) -> std::io::Result<()> {
    let am_uuid = uuid::Uuid::new_v4();
    let mut f = std::fs::File::create(path)?;
    writeln!(f, r#"<?xml version="1.0" encoding="UTF-8"?>"#)?;
    writeln!(
        f,
        r#"<AssetMap xmlns="http://www.smpte-ra.org/schemas/429-9/2007/AM">"#
    )?;
    writeln!(f, "  <Id>urn:uuid:{am_uuid}</Id>")?;
    writeln!(f, "  <Creator>IMF Wizard</Creator>")?;
    writeln!(f, "  <AssetList>")?;

    // PKL reference
    writeln!(f, "    <Asset>")?;
    writeln!(f, "      <Id>urn:uuid:{pkl_uuid}</Id>")?;
    writeln!(f, "      <ChunkList>")?;
    writeln!(f, "        <Chunk>")?;
    writeln!(f, "          <Path>PKL_{pkl_uuid}.xml</Path>")?;
    writeln!(f, "        </Chunk>")?;
    writeln!(f, "      </ChunkList>")?;
    writeln!(f, "    </Asset>")?;

    // CPL reference
    writeln!(f, "    <Asset>")?;
    writeln!(f, "      <Id>urn:uuid:{cpl_uuid}</Id>")?;
    writeln!(f, "      <ChunkList>")?;
    writeln!(f, "        <Chunk>")?;
    writeln!(f, "          <Path>CPL_{cpl_uuid}.xml</Path>")?;
    writeln!(f, "        </Chunk>")?;
    writeln!(f, "      </ChunkList>")?;
    writeln!(f, "    </Asset>")?;

    writeln!(f, "  </AssetList>")?;
    writeln!(f, "</AssetMap>")?;
    Ok(())
}
