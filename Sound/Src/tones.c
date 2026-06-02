#include "tones.h"

uint32_t Tone_Frequency_From_Key(char key)
{
    switch (key)
    {
        case '1': return 262;  /* C4 */
        case '2': return 294;  /* D4 */
        case '3': return 330;  /* E4 */
        case '4': return 349;  /* F4 */
        case '5': return 392;  /* G4 */
        case '6': return 440;  /* A4 */
        case '7': return 494;  /* B4 */
        case '8': return 523;  /* C5 */
        default:  return 0;
    }
}
