/*

      This file is part of the <goptical/core library.
  
      The <goptical/core library is free software; you can redistribute it
      and/or modify it under the terms of the GNU General Public
      License as published by the Free Software Foundation; either
      version 3 of the License, or (at your option) any later version.
  
      The <goptical/core library is distributed in the hope that it will be
      useful, but WITHOUT ANY WARRANTY; without even the implied
      warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
      See the GNU General Public License for more details.
  
      You should have received a copy of the GNU General Public
      License along with the <goptical/core library; if not, write to the
      Free Software Foundation, Inc., 59 Temple Place, Suite 330,
      Boston, MA 02111-1307 USA
  
      Copyright (C) 2010-2011 Free Software Foundation, Inc
      Author: Alexandre Becoulet

*/

/* -*- indent-tabs-mode: nil -*- */

#include <iostream>
#include <fstream>

#include <goptical/core/math/Vector>

#include <goptical/core/material/Abbe>

#include <goptical/core/sys/System>
#include <goptical/core/sys/Lens>
#include <goptical/core/sys/Source>
#include <goptical/core/sys/SourceRays>
#include <goptical/core/sys/SourcePoint>
#include <goptical/core/sys/Image>
#include <goptical/core/sys/Stop>

#include <goptical/core/curve/Sphere>
#include <goptical/core/shape/Disk>

#include <goptical/core/trace/Tracer>
#include <goptical/core/trace/Result>
#include <goptical/core/trace/Distribution>
#include <goptical/core/trace/Sequence>
#include <goptical/core/trace/Params>

#include <goptical/core/light/SpectralLine>

#include <goptical/core/analysis/RayFan>
#include <goptical/core/analysis/Spot>
#include <goptical/core/data/Plot>

#include <goptical/core/io/RendererSvg>
#include <goptical/core/io/Rgb>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

using namespace goptical;

int main(int argc, const char *argv[])
{
  //**********************************************************************
  // Optical system definition

  if (argc != 2) {
    fprintf(stderr, "Please supply a data file\n");
    exit(1);
  }
  FILE *fp = fopen(argv[1], "r");
  if (fp == NULL) {
    fprintf(stderr, "Unable to open file %s: %s\n", argv[1], strerror(errno));
    exit(1);
  }

  sys::system   sys;

  /* anchor lens */
  sys::Lens     lens(math::Vector3(0, 0, 0));

  double image_height = 42.42;
  double image_pos = 0;
  char line[128];
  while (fgets(line, sizeof line, fp) != NULL) {
    line[sizeof line - 1] = 0;
    if (line[0] == '#')
      continue;
    int stop = 0;
    double roc = 0, 
      ap_radius = 0, 
      thickness = 0, 
      refractive_index = 0, 
      abbe_vd = 0;
    sscanf(line, "%d %lf %lf %lf %lf %lf", &stop, &roc, &ap_radius, &thickness, &refractive_index, &abbe_vd);
    if (stop == 0) {
      image_height = roc;
      printf("Image height=%f\n", image_height);
    }
    else if (stop == 3) {
      lens.add_stop   (                ap_radius, thickness);
      printf("Added stop thickness=%f aperture_radius=%f\n",
        thickness, ap_radius);
    }
    else if (stop == 2) {
      //               roc,            ap.radius, thickness,
      lens.add_surface(roc,  ap_radius, thickness);
      printf("Added surface radius=%f thickness=%f aperture_radius=%f\n",
        roc, thickness, ap_radius);
    }
    else {
      //               roc,            ap.radius, thickness,
      lens.add_surface(roc,  ap_radius, thickness,
                   ref<material::AbbeVd>::create(refractive_index, abbe_vd));
      printf("Added surface radius=%f thickness=%f index=%f v no=%f aperture_radius=%f\n",
        roc, thickness, refractive_index, abbe_vd, ap_radius);
    }
    image_pos += thickness;
  }
  printf("Image position is at %f\n", image_pos);
  fclose(fp);

  sys.add(lens);
  /* anchor end */

  sys::Image      image(math::Vector3(0, 0, image_pos), image_height);
  sys.add(image);

  /* anchor sources */
  sys::SourceRays  source_rays(math::Vector3(0, 27.5, -1000));

  sys::SourcePoint source_point(sys::SourceAtFiniteDistance,
                                math::Vector3(0, 27.5, -1000));

  // add sources to system
  sys.add(source_rays);
  sys.add(source_point);

  // configure sources
  source_rays.add_chief_rays(sys);
  source_rays.add_marginal_rays(sys, 14);

  source_point.clear_spectrum();
  source_point.add_spectral_line(light::SpectralLine::C);
  source_point.add_spectral_line(light::SpectralLine::e);
  source_point.add_spectral_line(light::SpectralLine::F);
  /* anchor end */

  /* anchor seq */
  trace::Sequence seq(sys);

  sys.get_tracer_params().set_sequential_mode(seq);
  std::cout << "system:" << std::endl << sys;
  std::cout << "sequence:" << std::endl << seq;
  /* anchor end */

  //**********************************************************************
  // Drawing rays and layout

  {
    /* anchor layout */
    io::RendererSvg renderer("layout.svg", 800, 400);

#if 1
    // draw 2d system layout
    sys.draw_2d_fit(renderer);
    sys.draw_2d(renderer);
#else
    // draw 2d layout of lens only
    lens.draw_2d_fit(renderer);
    lens.draw_2d(renderer);
#endif

    trace::tracer tracer(sys);

    // trace and draw rays from rays source
    sys.enable_single<sys::Source>(source_rays);
    tracer.get_trace_result().set_generated_save_state(source_rays);

    tracer.trace();
    tracer.get_trace_result().draw_2d(renderer);
    /* anchor end */
  }

  {
    /* anchor spot */
    sys.enable_single<sys::Source>(source_point);

    sys.get_tracer_params().set_default_distribution(
      trace::Distribution(trace::HexaPolarDist, 12));

    analysis::Spot spot(sys);

    /* anchor end */
    {
    /* anchor spot */
      io::RendererSvg renderer("spot.svg", 300, 300, io::rgb_black);

      spot.draw_diagram(renderer);
    /* anchor end */
    }

    {
    /* anchor spot_plot */
      io::RendererSvg renderer("spot_intensity.svg", 640, 480);

      ref<data::Plot> plot = spot.get_encircled_intensity_plot(50);

      plot->draw(renderer);
    /* anchor end */
    }
  }

  {
    /* anchor opd_fan */
    sys.enable_single<sys::Source>(source_point);

    analysis::RayFan fan(sys);

    /* anchor end */
    {
    /* anchor opd_fan */
      io::RendererSvg renderer("opd_fan.svg", 640, 480);

      ref<data::Plot> fan_plot = fan.get_plot(analysis::RayFan::EntranceHeight,
                                              analysis::RayFan::OpticalPathDiff);

      fan_plot->draw(renderer);

    /* anchor end */
    }

    {
    /* anchor transverse_fan */
      io::RendererSvg renderer("transverse_fan.svg", 640, 480);

      ref<data::Plot> fan_plot = fan.get_plot(analysis::RayFan::EntranceHeight,
                                              analysis::RayFan::TransverseDistance);

      fan_plot->draw(renderer);

    /* anchor end */
    }

    {
    /* anchor longitudinal_fan */
      io::RendererSvg renderer("longitudinal_fan.svg", 640, 480);

      ref<data::Plot> fan_plot = fan.get_plot(analysis::RayFan::EntranceHeight,
                                              analysis::RayFan::LongitudinalDistance);

      fan_plot->draw(renderer);

    /* anchor end */
    }
  }

  return 0;
}

