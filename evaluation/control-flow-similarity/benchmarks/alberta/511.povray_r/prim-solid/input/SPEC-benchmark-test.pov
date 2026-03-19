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

#declare trans = 0;

#macro ShapeFinish()
  finish
  { reflection { 0.1 }
    diffuse 0.2
    ambient 0.3
    specular 5
    roughness 0.0001
  }
#end

#macro ShapeInterior()
  /*interior
  {
    fade_power 2.0
    fade_distance 3.0
    ior 1.5
    dispersion 1.01
    caustics 0.6
  }*/
#end

#macro ShapeSettings()
  ShapeFinish()
  ShapeInterior()
#end

torus
{
  1.0, 0.4
  rotate <75, 45, 0>
  translate <0.6, 1.5, 0.8>
  pigment { rgbf <1, 0, 0, trans> }
  ShapeSettings()
}

sphere
{
  <0.7, 0.5, -3>, 0.5
  pigment { rgbf <0, 1, 0, trans> }
  ShapeSettings()
}

cylinder
{
  <-0.8, 0.2, -2.2>, <-1.2, 1.4, -2.2> 0.5
  pigment { rgbf <0, 0, 1, trans> }
  ShapeSettings()
}
