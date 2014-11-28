We implement an embedded hand-held voice device based on uclinux, libeXosip2 library and oRTP library. 
The hand-held device is connected to the SIP server by WIFI network, and it implements the complete SIP and RTP protocol. Both the command and the voice are read from the serial input. The voice is encoded by G.726.
The code is tested against a standalone SIP server.

This is a first version of the VOIP system. However, it is not used for the following reasons:
1. My colleague did not manage to merge the SIP server to the overall system at the time...
2. We need some more functionalities that cannot be natively supported by the SIP-protocol. For example, the nodes are mobile, and then we need the hand-off messages. 
3. Since we have a privately-owned network, we decided to design a simple SIP-like protocol.