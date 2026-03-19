/* caption.pov version 1.0
 * Persistence of Vision Ray Tracer scene description file
 * POV-Ray Object Collection demo
 *
 * Copyright 2013 Richard Callwood III.  Some rights reserved.
 * This file is licensed under the terms of the CC-LGPL.
 * See http://creativecommons.org/licenses/LGPL/2.1/ for more information.
 *
 * Vers  Date         Comments
 * ----  ----         --------
 * 1.0   23-dec-2013  Created.
 */
// +a +am1 +j +r5
//UberPOV settings:
// +a +am3 +r4 +ac0.999999
// +a +am3 +r4 +ac1.0 (very slow)
#version 3.5;

#include "screen.inc"
#include "functions.inc"

#declare Pretrace = 1 / image_width;
global_settings
{ assumed_gamma 1
  radiosity
  { count 200
    error_bound 0.5
    pretrace_start 32 * Pretrace
    pretrace_end 2 * Pretrace
    recursion_limit 2
  }
}

camera {
  location <-0.2, 1.5, -7.2>
  look_at <0, 1, 0>
  focal_point<0, 0.8, -2.5>
  angle 45
  /*aperture 0.1
  blur_samples 200
  variance 0
  confidence 0.99*/
}

light_source
{
  <0, 2, -5>, rgb <2.2, 1.8, 1.3> spotlight
  point_at <0.1, 0.5, 2>
  falloff 60
  radius 45
}

light_source
{
  <0, 4, 10>, rgb <2.2, 1.8, 1.3> spotlight
  point_at <0.1, 0, 0>
  falloff 60
  radius 25
}


#declare C_SKY = rgb <0.421, 0.631, 1.263>;
sky_sphere
{ pigment
  { gradient y color_map
    { [0.0 C_SKY]
      [0.2 C_SKY * 0.6]
      [0.7 C_SKY * 0.3]
      [1.0 C_SKY * 0.2]
    }
  }
}

#default { finish { ambient 0 diffuse 0.75 } }

plane
{ y, 0
  pigment { checker rgb <0.34, 0.04, 0.07> rgb <1.0, 1.0, 0.5> scale 0.5 }
}

#macro ShapeFinish()
  finish
  { reflection { 0.5 metallic }
    diffuse 0.01
    ambient 0.1
    specular 1200 metallic
    roughness 0.0001
  }
#end

#macro ShapeInterior()
  interior
  {
  }
#end

#macro ShapeSettings()
  ShapeFinish()
  ShapeInterior()
#end

#declare fn_Pigm=function {
  pigment {
    dents
    color_map {
      [0 color rgb 0]
      [1 color rgb 1]
    }
  }
}

#declare trans = 0;
#declare bIntensity = 25;

isosurface
{
  function{
    f_sphere(x, y, z, 0.8)
    + fn_Pigm(x * bIntensity,
              y * bIntensity,
              z * bIntensity).gray * 0.1
    - f_noise3d(x, y, z) * 0.7
  }
  contained_by { sphere { 0, 1.5 } }
  translate <0, 1.2, -3>
  
  pigment { rgbf <0, 1, 0, trans> }
  ShapeSettings()
}
