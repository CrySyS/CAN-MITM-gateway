#ifndef RoleSelector_H
#define RoleSelector_H

#include "LED.h"
#include "Role.h"

/**
 * This class can negotiate the role of the device with the other role
 */
class RoleSelector {
   private:
    Role selectedRole;

   public:
    RoleSelector(LED* masterLED, LED* slaveLED);

    Role& getRole();
    static void holdSlaveInReset();
};

#endif