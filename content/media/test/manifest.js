// In each list of tests below, test file types that are not supported should
// be ignored. To make sure tests respect that, we include a file of type
// "bogus/duh" in each list.

// These are small test files, good for just seeing if something loads.
var gSmallTests = [
  { name:"r11025_s16_c1.wav", type:"audio/x-wav", duration:1.0 },
  { name:"320x240.ogv", type:"video/ogg", width:320, height:240 },
  { name:"bogus.duh", type:"bogus/duh" }
];

// These are files that we just want to make sure we can play through.
// We can also check metadata.
// Put files of the same type together in this list so if something crashes
// we have some idea of which backend is responsible.
var gPlayTests = [
  // file is truncated
  { name:"r11025_u8_c1_trunc.wav", type:"audio/x-wav", duration:1.8 },
  // file has trailing non-PCM data
  { name:"r11025_s16_c1_trailing.wav", type:"audio/x-wav", duration:1.0 },
  // file with list chunk
  { name:"r16000_u8_c1_list.wav", type:"audio/x-wav", duration:4.2 },
  // Ogg stream with eof marker
  { name:"bug461281.ogg", type:"application/ogg" },
  // oggz-chop stream
  { name:"bug482461.ogv", type:"video/ogg", duration:4.24 },
  { name:"bogus.duh", type:"bogus/duh" }
];

// These are files that should refuse to play and report an error,
// without crashing of course.
// Put files of the same type together in this list so if something crashes
// we have some idea of which backend is responsible.
var gErrorTests = [
  { name:"bogus.wav", type:"audio/x-wav" },
  { name:"bogus.ogv", type:"video/ogg" },
  { name:"bogus.duh", type:"bogus/duh" }
];

function checkMetadata(msg, e, test) {
  if (test.width) {
    is(e.videoWidth, test.width, msg + " video width");
  }
  if (test.height) {
    is(e.videoHeight, test.height, msg + " video height");
  }
  if (test.duration) {
    ok(Math.abs(e.duration - test.duration) < 0.1,
       msg + " duration should be around " + test.duration);
  }
}
