/*
    DePan plugin for Avisynth 2.5 - global motion estimation and compensation of camera pan
  Version 1.10.0, April 29, 2007
  (plugin interface)
  Copyright(c)2004-2007, A.G. Balakhnin aka Fizick
  bag@hotmail.ru

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  At the first stage we must estimate global motion (pan) in frames
  (with DePanEstimate function of DePanEstimate plugin (by phase-shift method),
  put it to special service clip)

  DePan at second stage shifts frame images for global motion compensation

  Plugin Functions:
    DePan - generates motion with motion data, previously calculated by DePanEstimate
      DePanInterleave function - generate long interleaved clip with motion compensated frames
    DePanStabilize function make some motion stabilization (deshake)
    DepanScenes function detects scenechanges

  v1.9 - Remove DePanEstimate function to separate plugin depanestimate.dll
*/

#include <avisynth.h>

//****************************************************************************
// This is the declaration of plugin functions

AVSValue __cdecl Create_DePan(AVSValue args, void* user_data, IScriptEnvironment* env);

AVSValue __cdecl Create_DePanInterleave(AVSValue args, void* user_data, IScriptEnvironment* env);

AVSValue __cdecl Create_DePanStabilize(AVSValue args, void* user_data, IScriptEnvironment* env);

AVSValue __cdecl Create_DePanScenes(AVSValue args, void* user_data, IScriptEnvironment* env);

//*****************************************************************************
// The following function is the function that actually registers the filter in AviSynth
// It is called automatically, when the plugin is loaded to see which functions this filter contains.
/* New 2.6 requirement!!! */
// Declare and initialise server pointers static storage.
const AVS_Linkage *AVS_linkage = 0;

/* New 2.6 requirement!!! */
// DLL entry point called from LoadPlugin() to setup a user plugin.
extern "C" __declspec(dllexport) const char* __stdcall
AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors) {

  /* New 2.6 requirment!!! */
  // Save the server pointers.
  AVS_linkage = vectors;
  env->AddFunction("DePan", "c[data]c[offset]f[subpixel]i[pixaspect]f[matchfields]b[mirror]i[blur]i[info]b[inputlog]s", Create_DePan, 0);
  env->AddFunction("DePanInterleave", "c[data]c[prev]i[next]i[subpixel]i[pixaspect]f[matchfields]b[mirror]i[blur]i[info]b[inputlog]s", Create_DePanInterleave, 0);
  env->AddFunction("DePanStabilize", "c[data]c[cutoff]f[damping]f[initzoom]f[addzoom]b[prev]i[next]i[mirror]i[blur]i[dxmax]f[dymax]f[zoommax]f[rotmax]f[subpixel]i[pixaspect]f[fitlast]i[tzoom]f[info]b[inputlog]s[vdx]s[vdy]s[vzoom]s[vrot]s[method]i[debuglog]s", Create_DePanStabilize, 0);
  env->AddFunction("DePanScenes", "c[plane]i[inputlog]s", Create_DePanScenes, 0);
  // The AddFunction has the following parameters:
    // AddFunction(Filtername , Arguments, Function to call,0);

    // Arguments is a string that defines the types and optional names of the arguments for you filter.
    // c - Video Clip
    // i - Integer number
    // f - float number
    // s - String
    // b - boolean

   // The word inside the [ ] lets you used named parameters in your script

  return "`DePan' DePan plugin";
  // A freeform name of the plugin.
}
