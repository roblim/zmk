---
title: Caps Word Behavior
sidebar_label: Caps Word
---

## Summary

The caps word behavior behaves similar to a caps lock, but will automatically deactivate when any key not in a continue list is pressed, or if the caps word key is pressed again. For smaller keyboards using [mod-taps](/docs/behaviors/mod-tap), this can help avoid repeated alternating holds when typing words in all caps.

The modifiers are applied only to to the alphabetic (`A` to `Z`) keycodes, to avoid automatically appliying them to numeric values, etc.

### Behavior Binding

- Reference: `&caps_word`

Example:

```
&caps_word
```

### Configuration

#### Continue List

By default, the caps word will remain active when any alphanumeric character or underscore (`UNDERSCORE`), backspace (`BACKSPACE`), or delete (`DELETE`) characters are pressed. Any other non-modifier keycode sent will turn off caps word. If you would like to override this, you can set a new array of keys in the `continue-list` property in your keymap:

```
&caps_word {
    continue-list = <UNDERSCORE MINUS>;
};

/ {
    keymap {
        ...
    };
};
```

#### Applied Modifier(s)

In addition, if you would like _multiple_ modifiers, instead of just `MOD_LSFT`, you can override the `mods` property:

```
&caps_word {
    mods = <(MOD_LSFT | MOD_LALT)>;
};

/ {
    keymap {
        ...
    };
};
```

#### Idle Timeout

By default, caps word turns off automatically if no keys are pressed for 5 seconds. This can be changed by setting the `idle-timeout-ms` property in your keymap. This value is in milliseconds.

For example, this would change the timeout to 10 seconds:

```
&caps_word {
    idle-timeout-ms = <10000>;
};
```

Setting the timeout to 0 configures caps word to never time out. It will remain active until you press a key that turns off caps word.

### Multiple Caps Breaks

If you want to use multiple caps breaks with different codes to break the caps, you can add additional caps words instances to use in your keymap:

```
/ {
    prog_caps: behavior_prog_caps_word {
        compatible = "zmk,behavior-caps-word";
        label = "PROG_CAPS";
        #binding-cells = <0>;
        continue-list = <UNDERSCORE>;
    };

    keymap {
        ...
    };
};
```
