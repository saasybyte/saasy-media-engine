.PHONY: configure reconfigure build build-speaking build-listening run-speaking run-listening \
        vendor-deps fetch-prebuilt clean

# Fetch prebuilt vendored deps from GitHub Releases (fast path; auto-detects platform)
fetch-prebuilt:
	./scripts/fetch-prebuilt.sh

# Configure cmake (auto-detects platform)
configure:
	./scripts/configure.sh

# Clean and reconfigure
reconfigure: clean configure

# Build both engines
build: build-speaking build-listening

# Build speaking engine
build-speaking:
	./scripts/build-speaking-engine.sh

# Build listening engine
build-listening:
	./scripts/build-listening-engine.sh

# Run speaking engine
run-speaking:
	./scripts/run-speaking-engine.sh

# Run listening engine
run-listening:
	./scripts/run-listening-engine.sh

# Vendor WebRTC deps (auto-detects platform)
vendor-deps:
ifeq ($(shell uname -s),Darwin)
	./scripts/vendor-webrtc-deps-macos.sh
else
	./scripts/vendor-webrtc-deps-linux.sh
endif

# Clean build artifacts
clean:
	rm -rf build
