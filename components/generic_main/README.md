# Bruce Perens' Generic Main for esp-idf.

This is a generic "main" program for the ESP-32 using the esp-idf toolkit. It
implements new embedded protocol
handlers for SSNTP, ICMPv6, STUN, PCP, REST, Dynamic DNS, a new embedded event-driven I/O
facility, and a new facility for submitting jobs to run in existing FreeRTOS threads.
This facility can support many ESP-32 applications, and removes the requirement for the 
developer to write a great deal of systems code before getting to their application.

On a Linux system, these facilities would have been available out-of-the-box, but on
the ESP-32 running FreeRTOS and LwIP, new implementations had to be coded for small
size and efficiency.

If you wish to embed
this facility in a proprietary application, you *must* pay the developer for a commercial
license. Contact Bruce Perens bruce@perens.com .
This facility is available for free *only* for Open Source applications, and only those
where the license of the entire program is compatible with AGPL3.

In order to compensate Bruce Perens for his work, this section of the "K6BP Rigcontrol"
application is dual-licensed and will be made available to proprietary developers for a
fee. Thus, the copyright of contributions to it must be assigned to Algorithmic LLC so
that it can continue to be dual-licensed. The copyright of other parts of "K6BP
Rigcontrol" may remain the property of the contributor.

Algorithmic LLC will covenant back to contributors the right to use *their contribution*
to this facility for any purpose - including embedding it in Open Source and proprietary
software.
