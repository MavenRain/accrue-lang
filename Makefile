# Builds the accrue-lang kernel on the tcc-wasm host with TinyCC. The kernel is
# the cairn-lang kernel at 0743108, which is a port of the intent-lang kernel at
# 67445ae. See README.md. TARGET (src/front/lower.c and src/wasm.c, on src/ir.h)
# is the Wasm target (S8, the cairn-lang port at 024c4cd); the tools leave it
# out.
TCC ?= tcc
CC ?= cc
CFLAGS = -std=c99 -Wall -Werror -Isrc
CLANG_FLAGS = -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only -Isrc
FRONT = src/front/base.c src/front/u256.c src/front/field.c src/front/secp256k1.c src/front/blsag.c src/front/elgamal.c src/front/threshold.c src/front/frost.c src/front/lexer.c src/front/parser.c src/front/front.c src/front/eval.c src/front/check.c src/front/fmt.c src/sha256.c
TARGET = src/front/lower.c src/wasm.c src/js.c src/json.c src/wgsl.c
SRC = src/main.c src/front/jsonin.c src/front/codes.c src/mcp.c $(FRONT) $(TARGET)
FRONT_TOOL = test/fronttool.c $(filter-out src/front/front.c,$(FRONT))
BLSAG_TOOL = test/blsagtool.c src/front/u256.c src/front/field.c src/front/secp256k1.c src/front/blsag.c src/sha256.c
ELGAMAL_TOOL = test/elgamaltool.c src/front/u256.c src/front/field.c src/front/secp256k1.c src/front/blsag.c src/front/elgamal.c src/sha256.c
THRESHOLD_TOOL = test/thresholdtool.c src/front/u256.c src/front/field.c src/front/secp256k1.c src/front/blsag.c src/front/elgamal.c src/front/threshold.c src/sha256.c
DKG_TOOL = test/dkgtool.c src/front/u256.c src/front/field.c src/front/secp256k1.c src/front/blsag.c src/front/elgamal.c src/front/threshold.c src/front/dkg.c src/sha256.c
FROST_TOOL = test/frosttool.c src/front/u256.c src/front/field.c src/front/secp256k1.c src/front/blsag.c src/front/elgamal.c src/front/threshold.c src/front/dkg.c src/front/frost.c src/sha256.c
SIGMA_TOOL = test/sigmatool.c src/front/u256.c src/front/field.c src/front/secp256k1.c src/front/blsag.c src/front/elgamal.c src/front/threshold.c src/front/sigma.c src/sha256.c
BALLOT_TOOL = test/ballottool.c src/front/u256.c src/front/field.c src/front/secp256k1.c src/front/blsag.c src/front/elgamal.c src/front/threshold.c src/front/sigma.c src/sha256.c
SECP_TOOL = test/secptool.c src/front/u256.c src/front/field.c src/front/secp256k1.c
SHA_TOOL = test/shatool.c src/sha256.c
U256_TOOL = test/u256tool.c src/front/u256.c
# The Wasm runner links the wasmtime C API. A missing file fails the build of
# build/wasmtool, so the gate fails; nothing is downloaded.
WASMTIME ?= /opt/homebrew
WASM_TOOL = test/wasmtool.c src/front/u256.c
# The JS runner (S9 P2, Q-S9-2 (a)) runs an ES module under node.
JS_TOOL = test/jstool.c src/front/u256.c
WASMTIME_FILES = $(WASMTIME)/include/wasmtime.h $(WASMTIME)/lib/libwasmtime.dylib
# The P5 no-trap check of each Wasm module (Q-S8-4 (a)).
NOTRAP_TOOL = test/notraptool.c
WGSL_TOOL = test/wgsltool.c src/front/u256.c
MCP_TOOL = test/mcptool.c
SKILL_TOOL = test/skilltool.c
PATH_TOOL = test/pathtool.c $(filter-out src/front/front.c,$(FRONT))
RUN_TOOL = test/runtool.c $(FRONT)
HEADERS = $(wildcard src/*.h src/front/*.h)

.PHONY: build check check-clang clean gpu-check

build: build/langc build/fronttool build/blsagtool build/elgamaltool build/thresholdtool build/dkgtool build/sigmatool build/frosttool build/ballottool build/secptool build/shatool build/u256tool build/pathtool build/runtool build/wasmtool build/notraptool build/jstool build/wgsltool build/jstest build/mcptool build/skilltool

build/domain.c: domain/domain.lang gen/embed.c
	mkdir -p build
	$(TCC) -run gen/embed.c domain/domain.lang build/domain.c

build/langc: $(SRC) $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o build/langc $(SRC) build/domain.c

build/fronttool: $(FRONT_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/fronttool $(FRONT_TOOL)

build/blsagtool: $(BLSAG_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/blsagtool $(BLSAG_TOOL)

build/elgamaltool: $(ELGAMAL_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/elgamaltool $(ELGAMAL_TOOL)

build/thresholdtool: $(THRESHOLD_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/thresholdtool $(THRESHOLD_TOOL)

build/dkgtool: $(DKG_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/dkgtool $(DKG_TOOL)

build/sigmatool: $(SIGMA_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/sigmatool $(SIGMA_TOOL)

build/frosttool: $(FROST_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/frosttool $(FROST_TOOL)

build/ballottool: $(BALLOT_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/ballottool $(BALLOT_TOOL)

build/secptool: $(SECP_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/secptool $(SECP_TOOL)

build/shatool: $(SHA_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/shatool $(SHA_TOOL)

build/u256tool: $(U256_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/u256tool $(U256_TOOL)

build/pathtool: $(PATH_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/pathtool $(PATH_TOOL)

build/runtool: $(RUN_TOOL) $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o build/runtool $(RUN_TOOL) build/domain.c

build/wasmtool: $(WASM_TOOL) $(HEADERS) $(WASMTIME_FILES)
	mkdir -p build
	$(TCC) $(CFLAGS) -I$(WASMTIME)/include -o build/wasmtool $(WASM_TOOL) $(WASMTIME)/lib/libwasmtime.dylib

build/notraptool: $(NOTRAP_TOOL)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/notraptool $(NOTRAP_TOOL)

build/wgsltool: $(WGSL_TOOL)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/wgsltool $(WGSL_TOOL)

build/jstool: $(JS_TOOL) $(HEADERS)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/jstool $(JS_TOOL)

build/jstest: test/jstest.c
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/jstest test/jstest.c

build/mcptool: $(MCP_TOOL)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/mcptool $(MCP_TOOL)

build/skilltool: $(SKILL_TOOL)
	mkdir -p build
	$(TCC) $(CFLAGS) -o build/skilltool $(SKILL_TOOL)

check-clang: build/domain.c
	$(CC) $(CLANG_FLAGS) $(SRC) build/domain.c
	$(CC) $(CLANG_FLAGS) $(FRONT_TOOL)
	$(CC) $(CLANG_FLAGS) $(BLSAG_TOOL)
	$(CC) $(CLANG_FLAGS) $(ELGAMAL_TOOL)
	$(CC) $(CLANG_FLAGS) $(THRESHOLD_TOOL)
	$(CC) $(CLANG_FLAGS) $(DKG_TOOL)
	$(CC) $(CLANG_FLAGS) $(SIGMA_TOOL)
	$(CC) $(CLANG_FLAGS) $(FROST_TOOL)
	$(CC) $(CLANG_FLAGS) $(BALLOT_TOOL)
	$(CC) $(CLANG_FLAGS) $(SECP_TOOL)
	$(CC) $(CLANG_FLAGS) $(SHA_TOOL)
	$(CC) $(CLANG_FLAGS) $(U256_TOOL)
	$(CC) $(CLANG_FLAGS) $(PATH_TOOL)
	$(CC) $(CLANG_FLAGS) $(RUN_TOOL) build/domain.c
	$(CC) $(CLANG_FLAGS) -isystem $(WASMTIME)/include $(WASM_TOOL)
	$(CC) $(CLANG_FLAGS) $(NOTRAP_TOOL)
	$(CC) $(CLANG_FLAGS) $(JS_TOOL)
	$(CC) $(CLANG_FLAGS) $(WGSL_TOOL)
	$(CC) $(CLANG_FLAGS) test/jstest.c
	$(CC) $(CLANG_FLAGS) $(MCP_TOOL)
	$(CC) $(CLANG_FLAGS) $(SKILL_TOOL)

check: build/langc build/fronttool build/blsagtool build/elgamaltool build/thresholdtool build/dkgtool build/sigmatool build/frosttool build/ballottool build/secptool build/shatool build/u256tool build/pathtool build/runtool build/wasmtool build/notraptool build/jstool build/wgsltool build/jstest build/mcptool build/skilltool check-clang
	sh test/gate.sh
	sh test/run.sh
	sh test/review.sh
	build/fronttool
	build/blsagtool
	build/elgamaltool
	build/thresholdtool
	build/dkgtool
	build/sigmatool
	build/frosttool
	build/frosttool pay
	build/ballottool
	build/ballottool onehot
	build/secptool
	build/shatool
	build/u256tool
	build/pathtool
	build/runtool
	build/langc --kit build test/wasm/hash.lang -o build/wasm-hash.wasm
	build/langc --kit abi test/wasm/hash.lang --text > build/wasm-hash.abi
	awk -F ' => ' '/^[^#]/ {print $$1}' test/wasm/hash.txt | build/wasmtool build/wasm-hash.wasm build/wasm-hash.abi > build/wasm-hash.out
	awk -F ' => ' '/^[^#]/ {print $$2}' test/wasm/hash.txt | cmp -s - build/wasm-hash.out && echo "wasm regressions: passed" || { echo "FAIL wasm regressions: build/wasm-hash.out is not the RESULT text of test/wasm/hash.txt"; exit 1; }
	build/langc --kit build test/wasm/secp.lang -o build/wasm-secp.wasm
	build/langc --kit abi test/wasm/secp.lang --text > build/wasm-secp.abi
	awk -F ' => ' '/^[^#]/ {print $$1}' test/wasm/secp.txt | build/wasmtool build/wasm-secp.wasm build/wasm-secp.abi > build/wasm-secp.out
	awk -F ' => ' '/^[^#]/ {print $$2}' test/wasm/secp.txt | cmp -s - build/wasm-secp.out && echo "wasm regressions: passed" || { echo "FAIL wasm regressions: build/wasm-secp.out is not the RESULT text of test/wasm/secp.txt"; exit 1; }
	build/langc --kit build test/wasm/traps.lang -o build/wasm-traps.wasm
	build/langc --kit abi test/wasm/traps.lang --text > build/wasm-traps.abi
	awk -F ' => ' '/^[^#]/ {print $$1}' test/wasm/traps.txt | build/wasmtool build/wasm-traps.wasm build/wasm-traps.abi > build/wasm-traps.out
	awk -F ' => ' '/^[^#]/ {print $$2}' test/wasm/traps.txt | cmp -s - build/wasm-traps.out || { echo "FAIL trap regressions: Wasm output differs"; exit 1; }
	awk -F ' => ' '/^[^#]/ {print $$1}' test/wasm/traps.txt | while IFS= read -r line; do set -- $$line; build/langc --kit eval test/wasm/traps.lang "$$@" 2>/dev/null || :; done > build/eval-traps.out
	awk -F ' => ' '/^[^#]/ {print $$2}' test/wasm/traps.txt | cmp -s - build/eval-traps.out && echo "trap regressions: passed" || { echo "FAIL trap regressions: evaluator output differs"; exit 1; }
	build/langc --kit build test/wasm/edge.lang -o build/wasm-edge.wasm
	build/langc --kit abi test/wasm/edge.lang --text > build/wasm-edge.abi
	awk -F ' => ' '/^[^#]/ {print $$1}' test/wasm/edge.txt | build/wasmtool build/wasm-edge.wasm build/wasm-edge.abi > build/wasm-edge.out
	awk -F ' => ' '/^[^#]/ {print $$2}' test/wasm/edge.txt | cmp -s - build/wasm-edge.out || { echo "FAIL edge regressions: Wasm output differs"; exit 1; }
	awk -F ' => ' '/^[^#]/ {print $$1}' test/wasm/edge.txt | while IFS= read -r line; do set -- $$line; build/langc --kit eval test/wasm/edge.lang "$$@" 2>/dev/null || :; done > build/eval-edge.out
	awk -F ' => ' '/^[^#]/ {print $$2}' test/wasm/edge.txt | cmp -s - build/eval-edge.out && echo "edge regressions: passed" || { echo "FAIL edge regressions: evaluator output differs"; exit 1; }
	build/langc --kit build test/wasm/hash.lang --js build/js-hash.mjs
	awk -F ' => ' '/^[^#]/ {print $$1}' test/wasm/hash.txt | build/jstool build/js-hash.mjs build/wasm-hash.abi > build/js-hash.out
	cmp -s build/wasm-hash.out build/js-hash.out || { echo "FAIL js regressions: build/js-hash.out differs from build/wasm-hash.out"; exit 1; }
	build/langc --kit build test/wasm/secp.lang --js build/js-secp.mjs
	awk -F ' => ' '/^[^#]/ {print $$1}' test/wasm/secp.txt | build/jstool build/js-secp.mjs build/wasm-secp.abi > build/js-secp.out
	cmp -s build/wasm-secp.out build/js-secp.out || { echo "FAIL js regressions: build/js-secp.out differs from build/wasm-secp.out"; exit 1; }
	build/langc --kit build test/wasm/traps.lang --js build/js-traps.mjs
	awk -F ' => ' '/^[^#]/ {print $$1}' test/wasm/traps.txt | build/jstool build/js-traps.mjs build/wasm-traps.abi > build/js-traps.out
	cmp -s build/wasm-traps.out build/js-traps.out || { echo "FAIL js regressions: build/js-traps.out differs from build/wasm-traps.out"; exit 1; }
	build/langc --kit build test/wasm/edge.lang --js build/js-edge.mjs
	awk -F ' => ' '/^[^#]/ {print $$1}' test/wasm/edge.txt | build/jstool build/js-edge.mjs build/wasm-edge.abi > build/js-edge.out
	cmp -s build/wasm-edge.out build/js-edge.out && echo "js regressions: passed" || { echo "FAIL js regressions: build/js-edge.out differs from build/wasm-edge.out"; exit 1; }
	build/langc --kit build test/js/names.lang --js 'build/js-names #?%.mjs'
	build/langc --kit abi test/js/names.lang --text > build/js-names.abi
	build/jstest
	build/langc --kit abi test/ir/transp.lang --text > build/transp.abi
	printf 'moved (nat) -> nat\n' | cmp -s - build/transp.abi || { echo "FAIL ABI: checked signatures must be available independently of lowering"; exit 1; }

# S10 P3: the WebGPU rows of test/gate.sh. Run it outside the sandbox (deno
# needs a GPU adapter).
gpu-check: build check-clang
	sh test/gate.sh gpu

clean:
	rm -rf build
