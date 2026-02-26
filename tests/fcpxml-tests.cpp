#include "bm-fcpxml-writer.hpp"

#include <iostream>

namespace {

void require(bool condition, const char *message)
{
	if (condition)
		return;
	std::cerr << "Test failed: " << message << std::endl;
	std::exit(1);
}

void test_time_conversion()
{
	require(bm::FcpxmlWriter::rational_time_from_frames(30, 30, 1) == "1/1s", "30fps exact second conversion");
	require(bm::FcpxmlWriter::rational_time_from_frames(3, 30000, 1001) == "1001/10000s",
		"NTSC frame rational conversion");
	require(bm::FcpxmlWriter::frame_duration_rational(30000, 1001) == "1001/30000s",
		"NTSC frame duration rational");
}

void test_artifact_paths()
{
	require(bm::FcpxmlWriter::artifact_path_for_media("/tmp/test.mov", bm::FcpxmlProfile::FinalCutClipMarkers) ==
			"/tmp/test.better-markers.fcp.fcpxml",
		"Final Cut artifact suffix");
	require(bm::FcpxmlWriter::artifact_path_for_media("/tmp/test.mov", bm::FcpxmlProfile::ResolveTimelineMarkers) ==
			"/tmp/test.better-markers.resolve.fcpxml",
		"Resolve artifact suffix");
}

void test_final_cut_profile_serialization()
{
	bm::FcpxmlDocumentInput input;
	input.profile = bm::FcpxmlProfile::FinalCutClipMarkers;
	input.media_path = "/tmp/final-cut.mov";
	input.fps_num = 30;
	input.fps_den = 1;

	bm::MarkerRecord marker;
	marker.start_frame = 45;
	marker.name = "Intro";
	marker.comment = "Add lower third";
	input.markers.push_back(marker);

	const bm::FcpxmlWriter writer;
	const QString xml = writer.build_document(input);

	require(xml.contains("<fcpxml version=\"1.11\">"), "FCPXML root version");
	require(xml.contains("<marker start=\"3/2s\" duration=\"1/30s\" value=\"Intro\" note=\"Add lower third\"/>"),
		"Final Cut clip marker serialization");
	require(!xml.contains("chapter-marker"), "chapter-marker not emitted in v1");
}

void test_resolve_profile_serialization()
{
	bm::FcpxmlDocumentInput input;
	input.profile = bm::FcpxmlProfile::ResolveTimelineMarkers;
	input.media_path = "/tmp/resolve.mp4";
	input.fps_num = 30000;
	input.fps_den = 1001;

	bm::MarkerRecord marker;
	marker.start_frame = 3;
	marker.name = "Cutaway";
	marker.comment = "Switch angle";
	input.markers.push_back(marker);

	const bm::FcpxmlWriter writer;
	const QString xml = writer.build_document(input);

	const qsizetype spine_marker_pos = xml.indexOf("<marker start=\"1001/10000s\"");
	const qsizetype asset_clip_pos = xml.indexOf("<asset-clip ");
	require(spine_marker_pos >= 0, "Resolve timeline marker present");
	require(asset_clip_pos >= 0, "Resolve asset clip present");
	require(spine_marker_pos < asset_clip_pos, "Resolve marker emitted at timeline/spine level");
}

} // namespace

int main()
{
	test_time_conversion();
	test_artifact_paths();
	test_final_cut_profile_serialization();
	test_resolve_profile_serialization();
	return 0;
}
