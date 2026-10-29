package main

import "testing"

func TestFlashImages(t *testing.T) {
	pr := []ghRelease{{Tag: "0.27.47", Draft: true, Assets: []ghAsset{{Name: "phosphor-0.27.47-sdcard-20261001.img.zip"}}},
		{Tag: "0.27.46-beta", Assets: []ghAsset{{Name: "phosphor-0.27.46-armhf.tar.gz"}, {Name: "phosphor-0.27.46-sdcard-20260912.img.zip"}}}}
	p, ok := phosphorImage(pr)
	if !ok || p.Kind != "phosphor" || p.Tag != "0.27.46-beta" || p.Label != "Phosphor OS" {
		t.Fatalf("phosphor: %+v %v", p, ok)
	}
}
