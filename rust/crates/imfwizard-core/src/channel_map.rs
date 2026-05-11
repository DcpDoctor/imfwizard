use serde::{Deserialize, Serialize};

/// Channel layout.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum ChannelLayout {
    Mono,
    Stereo,
    FiveOne,
    SevenOne,
    Atmos,
    Custom,
}

impl ChannelLayout {
    pub fn channel_count(&self) -> u32 {
        match self {
            Self::Mono => 1,
            Self::Stereo => 2,
            Self::FiveOne => 6,
            Self::SevenOne => 8,
            Self::Atmos | Self::Custom => 0,
        }
    }
}

/// Audio channel mapping.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ChannelMap {
    pub layout: ChannelLayout,
    pub channels: Vec<Channel>,
}

/// A single audio channel.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Channel {
    pub index: u32,
    pub label: String,
    pub mca_tag_symbol: String,
    pub mca_tag_name: String,
    pub language: String,
}

/// Standard 5.1 channel map.
pub fn channel_map_5_1() -> ChannelMap {
    ChannelMap {
        layout: ChannelLayout::FiveOne,
        channels: vec![
            Channel {
                index: 0,
                label: "Left".into(),
                mca_tag_symbol: "chL".into(),
                mca_tag_name: "Left".into(),
                language: String::new(),
            },
            Channel {
                index: 1,
                label: "Right".into(),
                mca_tag_symbol: "chR".into(),
                mca_tag_name: "Right".into(),
                language: String::new(),
            },
            Channel {
                index: 2,
                label: "Center".into(),
                mca_tag_symbol: "chC".into(),
                mca_tag_name: "Center".into(),
                language: String::new(),
            },
            Channel {
                index: 3,
                label: "LFE".into(),
                mca_tag_symbol: "chLFE".into(),
                mca_tag_name: "Low Frequency Effects".into(),
                language: String::new(),
            },
            Channel {
                index: 4,
                label: "Left Surround".into(),
                mca_tag_symbol: "chLs".into(),
                mca_tag_name: "Left Surround".into(),
                language: String::new(),
            },
            Channel {
                index: 5,
                label: "Right Surround".into(),
                mca_tag_symbol: "chRs".into(),
                mca_tag_name: "Right Surround".into(),
                language: String::new(),
            },
        ],
    }
}

/// Standard 7.1 channel map.
pub fn channel_map_7_1() -> ChannelMap {
    let mut map = channel_map_5_1();
    map.layout = ChannelLayout::SevenOne;
    map.channels.push(Channel {
        index: 6,
        label: "Left Back".into(),
        mca_tag_symbol: "chLrs".into(),
        mca_tag_name: "Left Rear Surround".into(),
        language: String::new(),
    });
    map.channels.push(Channel {
        index: 7,
        label: "Right Back".into(),
        mca_tag_symbol: "chRrs".into(),
        mca_tag_name: "Right Rear Surround".into(),
        language: String::new(),
    });
    map
}
