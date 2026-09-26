#!/usr/bin/env python3
"""Fast source contracts for the Keymap Editor-managed LisM configuration."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def layer_body(keymap: str, node_name: str) -> str:
    match = re.search(
        rf"^\s*{re.escape(node_name)}\s*\{{(?P<body>.*?)^\s*\}};",
        keymap,
        re.MULTILINE | re.DOTALL,
    )
    require(match is not None, f"missing keymap node {node_name}")
    return match.group("body")


def bindings(layer: str) -> list[str]:
    match = re.search(r"(?<!sensor-)bindings\s*=\s*<(?P<body>.*?)>;", layer, re.DOTALL)
    require(match is not None, "layer bindings are missing")
    result: list[str] = []
    for line in match.group("body").splitlines():
        starts = list(re.finditer(r"&[A-Za-z0-9_]+", line))
        for index, start in enumerate(starts):
            end = starts[index + 1].start() if index + 1 < len(starts) else len(line)
            result.append(re.sub(r"\s+", " ", line[start.start():end].strip()))
    return result


def sensor_binding(layer: str) -> str:
    match = re.search(r"sensor-bindings\s*=\s*<(?P<body>.*?)>;", layer, re.DOTALL)
    require(match is not None, "layer sensor binding is missing")
    return re.sub(r"\s+", " ", match.group("body").strip())


def main() -> int:
    keymap = read("config/lism.keymap")
    dtsi = read("boards/shields/lism/lism.dtsi")
    central_input = read("snippets/central-input.overlay")
    trackball_central = read("snippets/trackball-central/trackball.overlay")
    right_conf = read("config/lism_right.conf")
    west = read("config/west.yml")
    build_matrix = read("build.yaml")
    workflow = read(".github/workflows/build.yml")

    layer_nodes = ["default_layer", *(f"layer_{index}" for index in range(1, 10))]
    found_layers = re.findall(r"^\s*(default_layer|layer_\d+)\s*\{", keymap, re.MULTILINE)
    require(found_layers == layer_nodes, f"keymap is not exactly ten ordered layers: {found_layers}")
    layers = [layer_body(keymap, node) for node in layer_nodes]

    names = []
    for index, layer in enumerate(layers):
        match = re.search(r'display-name\s*=\s*"([^"]+)";', layer)
        require(match is not None, f"layer {index} has no Keymap Editor display name")
        names.append(match.group(1))
    require(
        names == ["Base", "Mouse", "Scroll", "Gesture 1", "Gesture 2",
                  "symbol", "number", "move", "setting", "User 9"],
        f"unexpected layer roles: {names}",
    )

    layer_bindings = [bindings(layer) for layer in layers]
    for index, items in enumerate(layer_bindings):
        require(len(items) == 48, f"layer {index} must retain all 48 editable slots: {len(items)}")

    mouse = layer_bindings[1]
    for mouse_button in ("MB1", "MB2", "MB3"):
        require(any(re.search(rf"\b{mouse_button}\b", item) for item in mouse),
                f"Mouse layer is missing {mouse_button}")
    require(mouse[19] == "&kp RIGHT_COMMAND",
            f"Mouse position 19 must be the requested right Command key: {mouse[19]}")
    configured_mouse_positions = [
        position for position, item in enumerate(mouse)
        if item.split()[0] not in {"&trans", "&none"}
    ]
    excluded = re.search(r"excluded-positions\s*=\s*<(?P<body>[^>]*)>;", central_input)
    require(excluded is not None, "AML excluded-positions are missing")
    excluded_positions = [int(value) for value in excluded.group("body").split()]
    require(
        excluded_positions == configured_mouse_positions,
        f"AML exclusions {excluded_positions} do not match Mouse bindings "
        f"{configured_mouse_positions}",
    )

    gesture_2_access = layer_bindings[0] + layer_bindings[1]
    require(any(re.match(r"&(?:lt|mo)\s+4\b", item) for item in gesture_2_access),
            "Gesture 2 must remain reachable from Base or Mouse")
    for layer_index in (3, 4):
        for position in (7, 16, 18, 27):
            behavior = layer_bindings[layer_index][position].split()[0]
            require(behavior not in {"&trans", "&none"},
                    f"Gesture layer {layer_index} action slot {position} is empty")

    base_text = " ".join(layer_bindings[0])
    require("&lt 5 LANGUAGE_1" in base_text and "&lt 6 SPACE" in base_text
            and "&lt 7 ENTER" in base_text,
            "Symbol/Number/Move layer-taps did not move with their roles")
    require(any(item == "&mo 8" for item in layer_bindings[5]),
            "Symbol-to-setting binding did not move with setting")

    actual_sensors = [sensor_binding(layer) for layer in layers]
    for index in (0, 1):
        require(actual_sensors[index].startswith("&inc_dec_kp "),
                f"Layer {index} lost the established two-step encoder behavior: "
                f"{actual_sensors[index]}")

    for label, layer_id in (("gesture_processor", 3), ("gesture_2_processor", 4)):
        processor = re.search(
            rf"{label}:\s*{label}\s*\{{(?P<body>.*?)\n\s*\}};",
            dtsi,
            re.DOTALL,
        )
        require(processor is not None, f"{label} node is missing")
        body = processor.group("body")
        for fragment in (
            f"layer = <{layer_id}>;", f"binding-layer = <{layer_id}>;",
            "up-position = <7>;", "left-position = <16>;",
            "right-position = <18>;", "down-position = <27>;",
            "threshold = <90>;", "cooldown-ms = <150>;", "reset-on-layer = <2>;",
        ):
            require(fragment in body, f"{label} contract missing: {fragment}")

    normalized_shared = re.sub(r"\s+", "", central_input)
    require(
        "input-processors=<&gesture_2_processor>,<&gesture_processor>,"
        "<&zip_temp_layer110000>;" in normalized_shared,
        "peripheral trackball Gesture 2/Gesture 1/AML order changed",
    )
    normalized_central = re.sub(r"\s+", "", trackball_central)
    require(
        "input-processors=<&zip_xy_transform(INPUT_TRANSFORM_X_INVERT|"
        "INPUT_TRANSFORM_Y_INVERT)>,<&gesture_2_processor>,<&gesture_processor>,"
        "<&zip_temp_layer110000>;" in normalized_central,
        "central trackball transform/Gesture 2/Gesture 1/AML order changed",
    )
    for fragment in (
        "report-interval-ms = <8>;", "pointer-acceleration-scroll-layer = <2>;",
        "pointer-acceleration-gesture-layer = <3>;",
        "pointer-acceleration-gesture-layer-2 = <4>;",
        "<&zip_scroll_scaler 1 16>",
    ):
        require(fragment in trackball_central, f"LisM trackball contract missing: {fragment}")

    expected_colors = [0, 7, 2, 3, 5, 4, 2, 6, 1, 3]
    require("CONFIG_RGBLED_WIDGET_SHOW_LAYER_COLORS=y" in right_conf,
            "right Central layer colors are not enabled")
    for layer_id, color in enumerate(expected_colors):
        require(f"CONFIG_RGBLED_WIDGET_LAYER_{layer_id}_COLOR={color}" in right_conf,
                f"layer {layer_id} LED color is not palette value {color}")
    for fragment in (
        "CONFIG_ZMK_SLEEP=y", "CONFIG_ZMK_IDLE_TIMEOUT=300000",
        "CONFIG_ZMK_IDLE_SLEEP_TIMEOUT=1800000", "CONFIG_CHARGE_INDICATOR=y",
    ):
        require(fragment in right_conf, f"right Central setting changed: {fragment}")

    require("revision: d24c1b8edd3dc09c1535ed0e32e336319f8dc9fc" in west,
            "PAW3222 dual-Gesture driver is not pinned")
    require("revision: e6b467792a1dabef8cfa5c9fd26bfefc9c16662b" in west,
            "validated RGB widget palette revision is not pinned")
    require(len(re.findall(r"^\s*- board:", build_matrix, re.MULTILINE)) == 10,
            "LisM build matrix is no longer the established ten targets")
    require("python3 scripts/verify-lism-config.py" in workflow,
            "source contracts are not enforced in Actions")
    require("'scripts/**'" in workflow, "script changes do not trigger Actions")

    print("verify-lism-config: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"verify-lism-config: FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
