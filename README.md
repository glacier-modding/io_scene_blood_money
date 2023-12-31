# Blood Money Blender addon
*Extract / View assets from the Blood Money game engine.*

# Usage
1. Load Blood Money Scene Zip File (M**_main.ZIP): File -> Import -> Blood Money Scene Zip (.zip)
2. Open viewport Blood Money menu and load single model or whole map using the filters
3. Profit!
 
## Requirements
 - Blender **3.4.0** or above

## Installation
 - Download the addon: **[Blood Money addon](https://github.com/glacier-modding/io_scene_blood_money/releases/download/v1.0.0/io_scene_blood_money.zip)**
 - Install the addon in blender like so:
   - go to *Edit > Preferences > Add-ons.*
   - use the *Install…* button and use the File Browser to select the `.zip`

## Credits
 * [ReGlacier (DronCode)](https://github.com/ReGlacier)
   * For making BMEdit which has been of great use for getting the scene tree, scene objects, primitive ids, parent/children relations with their transforms.

 * [pavledev](https://github.com/pavledev)
   * For making GlacierTEXEditor which has helped greatly in getting textures from the game.

Modified source code for both BMEdit and GlacierTEXEditor can be found in the [libraries](https://github.com/glacier-modding/io_scene_blood_money/tree/libraries) branch.

You'll also find the source code for BMExport which:
 - Uses BMEdit to get all the scene object information
 - Parses the material (MAT) and model (PRM) Blood Money files
 - Creates the unified scene.json and mat.json files used in the plugin
 - Generates the model OBJ/MTL files and folder structures
