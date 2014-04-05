/******************************************************************************
   STDR Simulator - Simple Two DImensional Robot Simulator
   Copyright (C) 2013 STDR Simulator
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
   
   Authors : 
   * Manos Tsardoulias, etsardou@gmail.com
   * Aris Thallas, aris.thallas@gmail.com
   * Chris Zalidis, zalidis@gmail.com 
******************************************************************************/

#include <stdr_robot/sensors/rfid_reader.h>

namespace stdr_robot {
  
  /**
  @brief Checks if an angle is between two others. Supposes that min < max
  @param target_ [float] The target angle
  @param min_ [float] min angle
  @param max_ [float] max angle
  @return true on success
  **/ 
  bool angCheck(float target_, float min_, float max_) 
  {
    int c = 0;
    c = (target_ + 2 * PI) / (2 * PI);
    float target = target_ + (1 - c) * PI * 2;
    c = (min_ + 2 * PI) / (2 * PI);
    float min = min_ + (1 - c) * PI * 2;
    c = (max_ + 2 * PI) / (2 * PI);
    float max = max_ + (1 - c) * PI * 2;
    
    if(min_ * max_ > 0) //!< Same sign
    {
      if(target > min && target < max)
      {
        return true;
      }
    }
    else
    {
      max += 2 * PI;
      if(target > min && target < max)
      {
        return true;
      }
      target += 2 * PI;
      if(target > min && target < max)
      {
        return true;
      }
    }
    return false;
  } 

  /**
  @brief Default constructor
  @param map [const nav_msgs::OccupancyGrid&] An occupancy grid map
  @param msg [const stdr_msgs::SonarSensorMsg&] The sonar description message
  @param name [const std::string&] The sensor frame id without the base
  @param n [ros::NodeHandle&] The ROS node handle
  @return void
  **/ 
  RfidReader::RfidReader(const nav_msgs::OccupancyGrid& map,
      const stdr_msgs::RfidSensorMsg& msg, 
      const std::string& name,
      ros::NodeHandle& n)
    : Sensor(map, name)
  {
    _description = msg;

    _timer = n.createTimer(
      ros::Duration(1 / msg.frequency), 
        &RfidReader::updateSensorCallback, this); 
         
    _tfTimer = n.createTimer(
      ros::Duration(1 / (2 * msg.frequency)), 
        &RfidReader::updateTransform, this);

    _publisher = n.advertise<stdr_msgs::RfidSensorMeasurementMsg>
      ( _namespace + "/" + msg.frame_id, 1 );
      
    rfids_subscriber_ = n.subscribe(
      "stdr_server/rfid_list", 
      1, 
      &RfidReader::receiveRfids,
      this);
  }
  
  /**
  @brief Default destructor
  @return void
  **/ 
  RfidReader::~RfidReader(void)
  {
    
  }

  /**
  @brief Updates the sensor measurements
  @return void
  **/ 
  void RfidReader::updateSensorCallback(const ros::TimerEvent&) 
  {
    if (!_gotTransform) { //!< wait for transform 
      return;
    }
    stdr_msgs::RfidSensorMeasurementMsg measuredTagsMsg;

    measuredTagsMsg.frame_id = _description.frame_id;

    if ( _map.info.height == 0 || _map.info.width == 0 )
    {
      ROS_DEBUG("In rfid reader : Outside limits\n");
      return;
    }
    
    float max_range = _description.maxRange;
    float sensor_th = tf::getYaw(_sensorTransform.getRotation());
    float min_angle = sensor_th - _description.angleSpan / 2.0;
    float max_angle = sensor_th + _description.angleSpan / 2.0;
    
    //!< Must implement the functionality
    for(unsigned int i = 0 ; i < rfid_tags_.rfid_tags.size() ; i++)
    {
      //!< Check for max distance
      float sensor_x = _sensorTransform.getOrigin().x();
      float sensor_y = _sensorTransform.getOrigin().y();
      float dist = sqrt(
        pow(sensor_x - rfid_tags_.rfid_tags[i].pose.x, 2) +
        pow(sensor_y - rfid_tags_.rfid_tags[i].pose.y, 2)
      );
      if(dist > max_range)
      {
        continue;
      }
      
      //!< Check for correct angle
      float ang = atan2(rfid_tags_.rfid_tags[i].pose.y - sensor_y,
        rfid_tags_.rfid_tags[i].pose.x - sensor_x);
      
      if(!angCheck(ang, min_angle, max_angle))
      {
        continue;
      }
      
      measuredTagsMsg.rfid_tags.push_back(rfid_tags_.rfid_tags[i]);
    }
    
    measuredTagsMsg.header.stamp = ros::Time::now();
    measuredTagsMsg.header.frame_id = _namespace + "_" + _description.frame_id;
    _publisher.publish( measuredTagsMsg );
  }

  /**
  @brief Returns the sensor pose relatively to robot
  @return geometry_msgs::Pose2D
  **/ 
  geometry_msgs::Pose2D RfidReader::getSensorPose()
  {
    return _description.pose;
  }
  
  /**
  @brief Returns the sensor frame id
  @return std::string
  **/ 
  std::string RfidReader::getFrameId()
  {
    return _namespace + "_" + _description.frame_id;
  }
  
  /**
  @brief Updates the sensor tf transform
  @return void
  **/
  void RfidReader::updateTransform(const ros::TimerEvent&)
  {
    try 
    {
      _tfListener.waitForTransform(
        "map_static",
        _namespace + "_" + _description.frame_id,
        ros::Time(0),
        ros::Duration(0.2));
        
      _tfListener.lookupTransform(
        "map_static",
        _namespace + "_" + _description.frame_id,
        ros::Time(0), 
        _sensorTransform);
      
      _gotTransform = true;
    }
    catch (tf::TransformException ex) 
    {
      ROS_DEBUG("%s", ex.what());
    }
  }
  
  /**
  @brief Receives the existent rfid tags
  **/
  void RfidReader::receiveRfids(const stdr_msgs::RfidTagVector& msg)
  {
    rfid_tags_ = msg;
  }
}
