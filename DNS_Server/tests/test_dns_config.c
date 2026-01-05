#include <stdio.h>
#include "dns_config.h"

int main()
{
    config_node *root = parse_config_file("../config/dns.conf");
    config_dump(root);

    return 0;
}