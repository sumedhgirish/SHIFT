
scenario:
	uv run scenario

secrets:
	uv run secrets --force ./global.secrets $$(cat group_ids.txt)

container:
	docker buildx build -t shift:latest ./firmware

hsm1_firmware:
	docker run --rm -v ./firmware:/hsm -v ./global.secrets:/secrets/global.secrets:ro -v ./build:/out -v /tmp/build:/tmp/build -e HSM_PIN=$$(cat pin_hsmA.txt) -e PERMISSIONS=$$(cat perms_hsmA.txt) shift:latest

hsm1: hsm1_firmware
	ectf hw /dev/ttyACM0 erase
	ectf hw /dev/ttyACM0 flash ./build/hsm.bin -n hsm1
	ectf hw /dev/ttyACM0 start

hsm2_firmware:
	docker run --rm -v ./firmware:/hsm -v ./global.secrets:/secrets/global.secrets:ro -v ./build:/out -v /tmp/build:/tmp/build -e HSM_PIN=$$(cat pin_hsmB.txt) -e PERMISSIONS=$$(cat perms_hsmB.txt) shift:latest

hsm2: hsm2_firmware
	ectf hw /dev/ttyACM2 erase
	ectf hw /dev/ttyACM2 flash ./build/hsm.bin -n hsm2
	ectf hw /dev/ttyACM2 start
