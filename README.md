# hyprNStack
This plugin is a modified version of Hyprland's Master layout. 

The primary change is that it allows an arbitrary number of non-master 'stacks'. This can be changed dynamically per-workspace.

The layout is sort of a combination of XMonad's 'MultiColumns' and Hyprland's Master layout.

# Configuration
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

### Configuration variable differences in comparison to Master Layout
*  `stacks` The number of *total* stacks, including the master.
*  `mfact` If this is set to 0 the master is the same size as the stacks. So if there is one master and 2 stacks they are all 1/3rd of the screen width(or height). Master and 3 stacks they are all 1/4th etc.
* `center_single_master` When there is a single window on the screen it is centered instead of taking up the entire monitor. This replaces the existing `always_center_master` and has slightly different behavior.

# Dispatchers

Two new dispatchers
 * `resetsplits` Reset all the window splits to default sizes.
 * `setstackcount` Change the number of stacks for the current workspace. Windows will be re-tiled to fit the new stack count.

Two new-ish orientations
 * `orientationhcenter` Master is horizontally centered with stacks to the left and right. 
 * `orientationvcenter` Master is vertically centered with stacks on the top and bottom. 
 * `orientationcenter` An alias for `orientationhcenter`
 

# Installing
Hyprland plugins basically have to be user-compiled and loaded. You probably need to compile and install hyprland yourself (or install from something like the AUR on Arch).
 
Once you've done that, follow these steps.

1. Export the environment variable HYPRLAND_HEADERS and point it at the root of the Hyprland source
   - `export HYPRLAND_HEADERS="$HOME/proj/Hyprland"`
2. Build hyprNStack
   - `make`
3. Copy the resulting nstackLayoutPlugin.so to some place
   - `cp nstackLayoutPlugin.so ~/.config/hypr/plugins`
4. Modify your hyprland.conf to load the plugin
   - `exec-once=hyprctl plugin load $HOME/.config/hypr/plugins/nstackLayoutPlugin.so`
5. Set your hyprland layout to `nstack`. 

# TODO
- [ ] Improve mouse resizing of stacks
- [X] Improve drag and drop rearranging of windows in and between stacks
- [X] Allow resizing of single master window
