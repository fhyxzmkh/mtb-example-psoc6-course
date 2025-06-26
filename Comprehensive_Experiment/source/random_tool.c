

#include "random_tool.h"

uint8_t password[PASSWORD_LENGTH + 1] = {0};

/*******************************************************************************
* Function Name: generate_password
********************************************************************************
* Summary: This function generates a 8 character long password
*
* Parameters:
*  None
*
* Return
*  void
*
*******************************************************************************/
void generate_password()
{
    int8_t index;
    uint32_t random_val;
    uint8_t temp_value = 0;

    /* Array to hold the generated password. Array size is inclusive of
       string NULL terminating character */
    memset(password, 0, PASSWORD_LENGTH + 1);

    cy_rslt_t result;

    cyhal_trng_t trng_obj;

    /* Initialize the TRNG generator block*/
    result = cyhal_trng_init(&trng_obj);

    if (result == CY_RSLT_SUCCESS)
    {

        for (index = 0; index < PASSWORD_LENGTH;)
        {
            /* Generate a random 32 bit number*/
            random_val = cyhal_trng_generate(&trng_obj);

            uint8_t bit_position  = 0;

            for(int8_t j=0;j<4;j++)
            {
                /* extract byte from the bit position offset 0, 8, 16, and 24. */
                temp_value=((random_val>>bit_position )& ASCII_7BIT_MASK);
                temp_value=check_range(temp_value);
                password[index++] = temp_value;
                bit_position  = bit_position  + 8;
            }
         }

        /* Terminate the password with end of string character */
        password[index] = '\0';

        /* Display the generated password on the UART Terminal */
        printf("One-Time Password: %s\r\n\n",password);

        /* Free the TRNG generator block */
        cyhal_trng_free(&trng_obj);
    }
}



/*******************************************************************************
* Function Name: check_range
********************************************************************************
* Summary: This function check if the generated random number is in the 
*          range of alpha-numeric, special characters ASCII codes.  
*          If not, convert to that range
*
* Parameters:
*  uint8_t
*
* Return
*  uint8_t 
*
*******************************************************************************/
// uint8_t check_range(uint8_t value)
// {
//     if (value < ASCII_VISIBLE_CHARACTER_START)
//     {
//          value += ASCII_VISIBLE_CHARACTER_START;
//     }
//      return value;
// }

uint8_t check_range(uint8_t value) {
    const char valid_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    value = valid_chars[value % (sizeof(valid_chars) - 1)];
    return value;
}