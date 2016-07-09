#include "angular_controller.h"

int main(int argc, char **argv) {
    std::shared_ptr<ros::NodeHandle> n; 
    ros::init(argc, argv, "move_node");
    
    //TODO make work for tortuga node?
    std::unique_ptr<RamNode> node;
    node.reset(new moveNode(n, 10));
    
    while (ros::ok()){
        node->update();
    }
    
}
