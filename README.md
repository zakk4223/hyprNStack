# hyprNStack
This plugins is a modified version of Hyprland's Master layout. 

The primary change is that it allows an arbitrary number of non-master 'stacks'. This can be changes on the fly per-workspace.

The layout is sort of a combination of XMonad's 'MultiColumns' and Hyprland's Master layout.

There are two-ish new orientations. hcenter and vcenter. Hcenter is like Master's existing 'center' orientation, except in the case of only two windows it is always side-by-side.

The 'always_center_master' setting has been replaced with 'center_single_master': When this is set to true a single window it is centered regardless of which orientation is being used.

Vcenter is just Hcenter, but the window is centered vertically with stacks above and below. Unlike Hyprland's existing Master layout, there is no way to produce a layout where the master is centered with only one stack on the side. (In other words you can't create a layout with a bunch of empty on the side of the master)

# Configuration
The plugin shares all the configuration keywords of the Master layout, but moved into the plugin:nstack:layout namespace.
"always_center_master" is removed. "mfact" and "center_single_master" are added.
When "mfact" is set to 0.0, the master stack will occupy equal space as other stacks. (So if you have three stacks it will be 0.33, four stacks 0.25 etc)

Default values are meant to produce a similar experience to the existing Master layout.
```
plugin {
  nstack {
    layout {
      orientation=left
      new_on_top=0
      new_is_master=1
      no_gaps_when_only=0
      special_scale_factor=0.8
      inherit_fullscreen=1
      stacks=2
      center_single_master=0
      mfact=0.5
    }
  }
}
```
# Dispatchers
All the same dispatchers as Master layout, with added 'orientationhcenter' and 'orientationvcenter'. 'orientationcenter' is aliased to 'orientationhcenter'
Two new dispatchers
 * "resetsplits": Reset all the window splits to default sizes.
 * "setstackcount": Change the number of stacks for the current workspace. Windows will be re-tiled to fit the new stack count.
 
# Installing
Hyprland plugins basically have to be user-compiled and loaded. See here: https://wiki.hyprland.org/Plugins/Development/Getting-Started/#making-your-first-plugin

Once you've prepared the Hyprland sources, you can do HYPRLAND_HEADERS=/path/to/hyprland/source make in the plugin directory (this repo). It should produce an nStackLayout.so file. Copy it somewhere and load it with hyprctl plugin load

# TODO
* Improve mouse resizing of stacks
* Improve drag and drop rearranging of windows in and between stacks
* Allow resizing of a single centered master window
