# Usage example:
# >../traffic_gen ex3.sc 11 >! ex3.ev
# >../sim ex3.ev 10
#

# create a network consisting of 1000 nodes
# a new node joins the system each 1 sec on the average
#
# 'events num avg wjoin wfail wroute
#  - num: number of events
#  - avg: average time between events (in ms)
#  - wjoin/(wjoin+wfail+wroute): fraction of nodes joinning the network
#  - wfail/(wjoin+wfail+wroute): fraction of node failures
#  - wroute/(wjoin+wfail+wroute): fraction of route requests
# 
events 1000 1000 1 0 0 

# fail 500 nodes
events 500 200 0 1 0

# wait 100 sec for the network to stabilize
wait 100000

# generate 500 route requests; the time
# between two events is 0.2 sec
events 500 200 0 0 1

# wait 1 sec to complete all operations 
wait 1000

# end the simulation
exit
