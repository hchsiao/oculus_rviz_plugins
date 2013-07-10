/*
 * Copyright (c) 2012, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <string>
#include <iostream>

#include <QWidget>
#include <QDesktopWidget>
#include <QApplication>

#include <OVR.h>

#include <boost/bind.hpp>

#include <OGRE/OgreRoot.h>
#include <OGRE/OgreSceneNode.h>
#include <OGRE/OgreRenderWindow.h>
#include <OGRE/OgreCompositionPass.h>
#include <OGRE/OgreCompositionTargetPass.h>
#include <OGRE/OgreCompositorInstance.h>

#include <ros/package.h>

#include <rviz/properties/bool_property.h>

#include <rviz/window_manager_interface.h>
#include <rviz/view_manager.h>
#include <rviz/render_panel.h>
#include <rviz/display_context.h>
#include <rviz/ogre_helpers/render_widget.h>
#include <rviz/ogre_helpers/render_system.h>

#include "rviz_oculus/oculus_display.h"
#include "rviz_oculus/ogre_oculus.h"

namespace rviz_oculus
{

OculusDisplay::OculusDisplay()
: render_widget_(0)
, scene_node_(0)
, oculus_(0)
{
  std::string rviz_path = ros::package::getPath(ROS_PACKAGE_NAME);
  Ogre::ResourceGroupManager::getSingleton().addResourceLocation( rviz_path + "/ogre_media", "FileSystem", ROS_PACKAGE_NAME );
  Ogre::ResourceGroupManager::getSingleton().initialiseResourceGroup(ROS_PACKAGE_NAME);

  fullscreen_property_ = new rviz::BoolProperty( "Render to Oculus", false,
    "If checked, will render fullscreen on your secondary screen. Otherwise, shows a window.",
    this, SLOT(onFullScreenChanged()));

  connect( QApplication::desktop(), SIGNAL( screenCountChanged ( int ) ), this, SLOT( onScreenCountChanged(int)) );
}

OculusDisplay::~OculusDisplay()
{
  oculus_->shutDownOculus();
  oculus_->shutDownOgre();
  render_widget_->close();

  delete oculus_;
}


void OculusDisplay::onScreenCountChanged( int newCount )
{
  if ( newCount == 1 )
  {
    fullscreen_property_->setBool(false);
    fullscreen_property_->setReadOnly(true);
  }
  else
  {
    fullscreen_property_->setReadOnly(false);
  }
}


void OculusDisplay::onFullScreenChanged()
{
  if ( fullscreen_property_->getBool() )
  {
    QRect screen_res = QApplication::desktop()->screenGeometry(1);
    //render_widget->setWindowFlags();
    render_widget_->setGeometry( screen_res );
    //render_widget->show();
    render_widget_->showFullScreen();
  }
  else
  {
    int x_res = 1280;
    int y_res = 800;
    if ( oculus_ )
    {
      OVR::HMDInfo info;
      oculus_->getHMDDevice()->GetDeviceInfo( &info );
      x_res = info.HResolution;
      y_res = info.VResolution;
    }
    int primary_screen = QApplication::desktop()->primaryScreen();
    QRect screen_res = QApplication::desktop()->screenGeometry( primary_screen );
    render_widget_->setGeometry( screen_res.x(), screen_res.y(), x_res, y_res );
    render_widget_->showNormal();
  }
}

void OculusDisplay::onInitialize()
{
  render_widget_ = new rviz::RenderWidget( rviz::RenderSystem::get() );
  render_widget_->setWindowTitle( "Oculus View" );

  render_widget_->setParent( context_->getWindowManager()->getParentWindow() );
  render_widget_->setWindowFlags( Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowMaximizeButtonHint );

  Ogre::RenderWindow *window = render_widget_->getRenderWindow();
  window->setVisible(true);
  window->setAutoUpdated(true);

  scene_node_ = scene_manager_->getRootSceneNode()->createChildSceneNode();

  oculus_ = new Oculus();
  onEnable();

  onScreenCountChanged( QApplication::desktop()->numScreens() );
  onFullScreenChanged();

  oculus_->setupOgre( scene_manager_, window, scene_node_ );

  update(0,0);
}


void OculusDisplay::preRenderTargetUpdate(const Ogre::RenderTargetEvent& evt)
{
}

void OculusDisplay::postRenderTargetUpdate(const Ogre::RenderTargetEvent& evt)
{
}

void OculusDisplay::onEnable()
{
  render_widget_->setVisible(true);
  if ( oculus_ )
  {
    oculus_->setupOculus();
  }
}

void OculusDisplay::onDisable()
{
  render_widget_->close();
  if ( oculus_ )
  {
    oculus_->shutDownOculus();
  }
}

void OculusDisplay::update( float wall_dt, float ros_dt )
{
  updateCamera();

  Ogre::ColourValue bg_color = context_->getViewManager()->getRenderPanel()->getViewport()->getBackgroundColour();
  static int i = 0;
  i++;

  oculus_->getCompositor(0)->getTechnique()->getOutputTargetPass()->getPass(0)->setClearColour(bg_color);
  oculus_->getViewport(0)->setBackgroundColour( bg_color );
  oculus_->getCompositor(1)->getTechnique()->getOutputTargetPass()->getPass(0)->setClearColour(bg_color);
  oculus_->getViewport(1)->setBackgroundColour( bg_color );

//  CompositorManager::getSingleton().getCompositorChain(oculus_->getViewport(0))->_getOriginalSceneCompositor()->getTechnique()->getOutputTargetPass()->getPass(0)->setClearColour(fadeColour);
//  oculus_->getViewport(i%2)->setBackgroundColour( Ogre::ColourValue(0,0,0) );
//  oculus_->getViewport(i%2)->setBackgroundColour( bg_color );
//  oculus_->getViewport(i%2)->setClearEveryFrame(true);
}

void OculusDisplay::updateCamera()
{
  const Ogre::Camera *cam = context_->getViewManager()->getCurrent()->getCamera();
  scene_node_->setPosition( cam->getDerivedPosition() );
  scene_node_->setOrientation( cam->getDerivedOrientation() );
  oculus_->update();
}

void OculusDisplay::reset()
{
  rviz::Display::reset();
}

} // namespace rviz

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS( rviz_oculus::OculusDisplay, rviz::Display )
