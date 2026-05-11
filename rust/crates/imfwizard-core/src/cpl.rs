use std::io::Write;
use std::path::Path;

use crate::imp::ImpOptions;

/// Write a CPL (Composition Playlist) XML file.
pub fn write_cpl(path: &Path, cpl_uuid: &str, opts: &ImpOptions) -> std::io::Result<()> {
    let mut f = std::fs::File::create(path)?;
    writeln!(f, r#"<?xml version="1.0" encoding="UTF-8"?>"#)?;
    writeln!(
        f,
        r#"<CompositionPlaylist xmlns="http://www.smpte-ra.org/schemas/2067-3/2016">"#
    )?;
    writeln!(f, "  <Id>urn:uuid:{cpl_uuid}</Id>")?;
    writeln!(
        f,
        "  <ContentTitle>{}</ContentTitle>",
        xml_escape(&opts.title)
    )?;
    writeln!(
        f,
        "  <ContentKind>{}</ContentKind>",
        if opts.content_kind.is_empty() {
            "feature"
        } else {
            &opts.content_kind
        }
    )?;
    writeln!(
        f,
        "  <EditRate>{} {}</EditRate>",
        opts.fps_num,
        if opts.fps_den == 0 { 1 } else { opts.fps_den }
    )?;
    writeln!(f, "  <SegmentList>")?;
    writeln!(f, "    <Segment>")?;
    writeln!(f, "      <Id>urn:uuid:{}</Id>", uuid::Uuid::new_v4())?;
    writeln!(f, "      <SequenceList/>")?;
    writeln!(f, "    </Segment>")?;
    writeln!(f, "  </SegmentList>")?;
    writeln!(f, "</CompositionPlaylist>")?;
    Ok(())
}

fn xml_escape(s: &str) -> String {
    s.replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
}
