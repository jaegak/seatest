#include "ros/ros.h"
#include "std_msgs/Int16.h"
#include "std_msgs/String.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sstream>
#include <iostream>

#include <net/if.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#define _USE_MATH_DEFINES
#include <math.h>


#define CANID_DELIM '#'
#define DATA_SEPERATOR '.'

std::string desired_hex = "0x00";
std::string desired_hex2 = "0x00";
int s;

using namespace std;

string Control_Endless = "3F00";
string Control_Enable = "0F00";

unsigned short COB_ID[4] = {0x201, 0x301, 0x401, 0x501};

unsigned char asc2nibble(char c) {

    if ((c >= '0') && (c <= '9'))
        return c - '0';

    if ((c >= 'A') && (c <= 'F'))
        return c - 'A' + 10;

    if ((c >= 'a') && (c <= 'f'))
        return c - 'a' + 10;

    return 16; /* error */
}

int hexstring2data(char *arg, unsigned char *data, int maxdlen) {

    int len = strlen(arg);
    int i;
    unsigned char tmp;

    if (!len || len%2 || len > maxdlen*2)
        return 1;

    memset(data, 0, maxdlen);

    for (i=0; i < len/2; i++) {

        tmp = asc2nibble(*(arg+(2*i)));
        if (tmp > 0x0F)
            return 1;

        data[i] = (tmp << 4);

        tmp = asc2nibble(*(arg+(2*i)+1));
        if (tmp > 0x0F)
            return 1;

        data[i] |= tmp;
    }

    return 0;
}

int parse_canframe(char *cs, struct canfd_frame *cf) {
    /* documentation see lib.h */

    int i, idx, dlen, len;
    int maxdlen = CAN_MAX_DLEN;
    int ret = CAN_MTU;
    unsigned char tmp;

    len = strlen(cs);
    //printf("'%s' len %d\n", cs, len);

    memset(cf, 0, sizeof(*cf)); /* init CAN FD frame, e.g. LEN = 0 */

    if (len < 4)
        return 0;

    if (cs[3] == CANID_DELIM) { /* 3 digits */

        idx = 4;
        for (i=0; i<3; i++){
            if ((tmp = asc2nibble(cs[i])) > 0x0F)
                return 0;
            cf->can_id |= (tmp << (2-i)*4);
        }

    } else if (cs[8] == CANID_DELIM) { /* 8 digits */

        idx = 9;
        for (i=0; i<8; i++){
            if ((tmp = asc2nibble(cs[i])) > 0x0F)
                return 0;
            cf->can_id |= (tmp << (7-i)*4);
        }
        if (!(cf->can_id & CAN_ERR_FLAG)) /* 8 digits but no errorframe?  */
            cf->can_id |= CAN_EFF_FLAG;   /* then it is an extended frame */

    } else
        return 0;

    if((cs[idx] == 'R') || (cs[idx] == 'r')){ /* RTR frame */
        cf->can_id |= CAN_RTR_FLAG;

        /* check for optional DLC value for CAN 2.0B frames */
        if(cs[++idx] && (tmp = asc2nibble(cs[idx])) <= CAN_MAX_DLC)
            cf->len = tmp;

        return ret;
    }

    if (cs[idx] == CANID_DELIM) { /* CAN FD frame escape char '##' */

        maxdlen = CANFD_MAX_DLEN;
        ret = CANFD_MTU;

        /* CAN FD frame <canid>##<flags><data>* */
        if ((tmp = asc2nibble(cs[idx+1])) > 0x0F)
            return 0;

        cf->flags = tmp;
        idx += 2;
    }

    for (i=0, dlen=0; i < maxdlen; i++){

        if(cs[idx] == DATA_SEPERATOR) /* skip (optional) separator */
            idx++;

        if(idx >= len) /* end of string => end of data */
            break;

        if ((tmp = asc2nibble(cs[idx++])) > 0x0F)
            return 0;
        cf->data[i] = (tmp << 4);
        if ((tmp = asc2nibble(cs[idx++])) > 0x0F)
            return 0;
        cf->data[i] |= tmp;
        dlen++;
    }
    cf->len = dlen;

    return ret;
}

void LogInfo(string message)
{
    cout << message << endl;
}


template <typename T>
std::string dec_to_hex(T dec, int NbofByte){
    std::stringstream stream_HL;
    string s, s_LH;
    stream_HL << std::setfill ('0') << std::setw(sizeof(T)*2) <<std::hex << dec;
    s = stream_HL.str();
    for (int i=0; i<NbofByte; i++){
        s_LH.append(s.substr(2*(NbofByte-1-i),2));
    }
    return s_LH;
}

int hexarray_to_int(unsigned char *buffer){
//    int length = sizeof(buffer)/ sizeof(*buffer);
    int length = 4;
    int hextoint = 0;
    for (int i=0; i<length; i++)
    {
        hextoint += (buffer[i] << 8*i);
    }

    return hextoint;
}

std::string stringappend(string a, string b)
{
    string s;
    s.append(a);
    s.append(b);
    return s;
}

void commandCallback(const std_msgs::Int16::ConstPtr& msg)
{
    int A = msg->data;
    int B = msg->data;
     desired_hex= dec_to_hex(msg->data ,16);
     desired_hex2= dec_to_hex(msg->data ,16);
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "motor_listen_sensor_talk");
    ros::NodeHandle n;
    ros::Publisher encoder_pub = n.advertise<std_msgs::Int16>("encoder", 1000);
    ros::Publisher magnetic_pub = n.advertise<std_msgs::Int16>("magnetic", 1000);
    string rtr;

    struct sockaddr_can addr;
    struct can_frame frame;
    struct canfd_frame frame_fd;
    int required_mtu;
    struct ifreq ifr;
    string data;

    struct canfd_frame frame_get;

    const char *ifname = "slcan0";
    struct msghdr canmsg;

    if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        perror("Error while opening socket");
        return -1;
    }

    strcpy(ifr.ifr_name, ifname);
    ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    printf("%s at index %d\n", ifname, ifr.ifr_ifindex);

//    setsockopt(s, SOL_CAN_RAW, CAN_RAW_FILTER, NULL, 0);

    if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Error in socket bind");
        return -2;
    }

    // Initiallize NMT Services
    LogInfo("Initialize NMT Services");
    // Reset Communication
    frame.can_id  = 0x000;
    frame.can_dlc = 2;
    frame.data[0] = 0x82;
    frame.data[1] = 0x00;
    write(s, &frame, sizeof(struct can_frame));
    LogInfo("Reset Communication");
    ros::Duration(2).sleep();

    // Start Remote Node
    frame.can_id  = 0x000;
    frame.can_dlc = 2;
    frame.data[0] = 0x01;
    frame.data[1] = 0x00;
    write(s, &frame, sizeof(struct can_frame));
    LogInfo("Start Remote Node");
    ros::Duration(1).sleep();

    ros::Subscriber sub = n.subscribe("desire_position", 1000, commandCallback);
    ros::spin();


    // Motor move
    frame.can_id  = 0x401;
    frame.can_dlc = 6;
    frame.data[0] = 0x7F;
    frame.data[1] = 0x00;
    frame.data[2] = 0x10;
    frame.data[3] = 0x27;
    frame.data[4] = 0x00;
    frame.data[5] = 0x00;
    write(s, &frame, sizeof(struct can_frame));
    ros::Duration(1).sleep();

    // Request PDO1
    stringstream sss;
    rtr = stringappend("281", "#R");
    required_mtu = parse_canframe((char*)(rtr.c_str()), &frame_fd);
    write(s, &frame_fd, required_mtu);
    sleep(0.0001);
    recvmsg(s, &canmsg, 0);
    std_msgs::Int16 msg;
    msg.data = hexarray_to_int(frame_get.data);

    encoder_pub.publish(msg);

    rtr = stringappend("381", "#R");
    required_mtu = parse_canframe((char*)(rtr.c_str()), &frame_fd);
    write(s, &frame_fd, required_mtu);
    sleep(0.0001);
    recvmsg(s, &canmsg, 0);
    std_msgs::Int16 msg2;
    msg2.data = hexarray_to_int(frame_get.data);

    magnetic_pub.publish(msg2);

    // Reset Communication
    frame.can_id  = 0x000;
    frame.can_dlc = 2;
    frame.data[0] = 0x82;
    frame.data[1] = 0x00;
    write(s, &frame, sizeof(struct can_frame));
    LogInfo("Reset Communication");
    ros::Duration(2).sleep();

    // Stop Remote Node
    frame.can_id  = 0x000;
    frame.can_dlc = 2;
    frame.data[0] = 0x02;
    frame.data[1] = 0x00;
    write(s, &frame, sizeof(struct can_frame));
    LogInfo("Stop Remote Node");

    return 0;
}
