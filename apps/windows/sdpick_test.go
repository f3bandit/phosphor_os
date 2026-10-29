package main

import "testing"

func TestFlashImages(t *testing.T) {
	rr := []ghRelease{{Tag: "1.2", Assets: []ghAsset{{Name: "SuperStation_1.2.0-ConsoleMode.img.zip"}, {Name: "SuperStation_1.2.0.img.zip"}}}, {Tag: "1.1", Assets: []ghAsset{{Name: "linux.img"}, {Name: "SuperStation_1.1.4.img.zip"}}}}
	got := pickImages(rr)
	if len(got) != 1 || got[0].Kind != "regular" || got[0].Name != "SuperStation_1.2.0.img.zip" {
		t.Fatalf("regular: %+v", got)
	}
	if got := pickImages([]ghRelease{{Tag: "v2", Assets: []ghAsset{{Name: "ConsoleMode.img.zip"}}}}); len(got) != 0 {
		t.Fatalf("console mode offered: %+v", got)
	}
	pr := []ghRelease{{Tag: "0.27.47", Draft: true, Assets: []ghAsset{{Name: "phosphor-0.27.47-sdcard-20261001.img.zip"}}},
		{Tag: "0.27.46-beta", Assets: []ghAsset{{Name: "phosphor-0.27.46-armhf.tar.gz"}, {Name: "phosphor-0.27.46-sdcard-20260912.img.zip"}}}}
	p, ok := phosphorImage(pr)
	if !ok || p.Kind != "phosphor" || p.Tag != "0.27.46-beta" || p.Label != "Phosphor OS" {
		t.Fatalf("phosphor: %+v %v", p, ok)
	}
}
