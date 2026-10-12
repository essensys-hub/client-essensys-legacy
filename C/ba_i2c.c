#include <mqx.h>
#include <bsp.h>
#include <timer.h>
#include <i2c.h>
#include <message.h>
#include <mutex.h>

#include "application.h"

#include "TableEchange.h"
#include "global.h"
#include "EspionRS.h"
#include "crc.h"
#include "i2c_bounded.h"
#include "ba_resil.h"

#define extern
#include "ba_i2c.h"
#undef extern


// Calcule le checksum du buffer passé en paramètre - Plus utilisé -> remplacé par un CRC
/*unsigned char uc_fct_Calcul_CheckSum_I2C(unsigned char *p_uc_buffer, unsigned short us_NbOctets)
{
	unsigned short l_us_i;
	unsigned char l_uc_CheckSum = 0;

	for(l_us_i=0; l_us_i<us_NbOctets; l_us_i++)
	{
		l_uc_CheckSum += p_uc_buffer[l_us_i];
	}
	l_uc_CheckSum = (uchar) ~l_uc_CheckSum;
	l_uc_CheckSum++;
	
	return (l_uc_CheckSum);
}*/

//--------------------------------------------------------------------------
// Feature i2c-surge-protection-2026-10-007 (Refs essensys-hub/essensys-board-SC944D#1)
//
// Reinitialisation du canal I2C (tache 5.3) : 099-37 fermait le canal puis en
// ouvrait un autre meme si fclose echouait (fuite d'un descripteur a chaque
// essai) et poursuivait la transaction avec le pointeur passe en parametre,
// perime apres la reinitialisation. La reouverture passe maintenant par
// bar_i2c_reopen() (un seul descripteur vivant, teste sur hote : NR-gcc-4) et
// la transaction utilise toujours fd_BA_I2C.
//--------------------------------------------------------------------------
static void *pv_I2C_Ouvrir(void *pv_User)
{
	MQX_FILE_PTR l_fd;

	(void)pv_User;
	vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\tOpen I2C\n");
	l_fd = fopen(I2C_DEVICE_POLLED, NULL);
	if(l_fd == NULL)
	{
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\terror opening the I2C driver!\n");
	}
	return (void *)l_fd;
}

static int i_I2C_Fermer(void *pv_User, void *pv_Handle)
{
	signed long l_sl_result;

	(void)pv_User;
	vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\tClose I2C\n");
	l_sl_result = (int_32)fclose((MQX_FILE_PTR)pv_Handle);
	if(l_sl_result)
	{
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\terror during close, returned: %d -> descripteur conserve\n", l_sl_result);
		ul_EspionTacheBANbEchecsFermetureI2C++;
	}
	// Tempo - trouvé dans forum freescale
	_time_delay(10);
	return (l_sl_result == 0) ? 0 : -1;
}

static int i_I2C_Configurer(void *pv_User, void *pv_Handle)
{
	MQX_FILE_PTR l_fd = (MQX_FILE_PTR)pv_Handle;
	signed long l_sl_param;
	signed long l_sl_code_result;

	(void)pv_User;
	l_sl_param = 50000;
	vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\tConfig vitesse\n");
	l_sl_code_result = ioctl(l_fd, IO_IOCTL_I2C_SET_BAUD, &l_sl_param);
	if(l_sl_code_result != I2C_OK)
	{
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\terror during setting baudrate %d\n", l_sl_code_result);
		return -1;
	}
	vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\tConfig master\n");
	l_sl_code_result = ioctl(l_fd, IO_IOCTL_I2C_SET_MASTER_MODE, NULL);
	if(l_sl_code_result != I2C_OK)
	{
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\terror during master configuration %d\n", l_sl_code_result);
		return -1;
	}
	l_sl_param = I2C_BUS_ADDRESS_BP;	// adresse du boitier principal
	vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\tConfig adresse\n");
	l_sl_code_result = ioctl(l_fd, IO_IOCTL_I2C_SET_STATION_ADDRESS, &l_sl_param);
	if(l_sl_code_result != I2C_OK)
	{
		vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\terror during address configuration %d\n", l_sl_code_result);
		return -1;
	}
	return 0;
}

static const bar_i2c_ops_t s_st_I2C_Ops = { pv_I2C_Ouvrir, i_I2C_Fermer, i_I2C_Configurer, NULL };

// Budget de transaction (20 ms, i2c_bounded.h) : une transaction qui le
// depasse se termine en erreur au lieu de prolonger le dialogue.
#define VERIF_BUDGET_TRANSACTION()	if(l_sl_code_result == I2C_OK && i2cb_txn_expired() != 0) \
									{ \
										l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_BLOCAGE; \
										DETECTION_ERREUR_DIALOGUE_BA_EMISSION_COMPLETE(uc_BANumber); \
										vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : budget de transaction depasse BA %i\n", uc_BANumber); \
									}

// Writes the provided data buffer at slave address and reads the slave status
signed long sl_fct_write_polled(MQX_FILE_PTR fd,				// [IN] Ignore (conserve pour compatibilite) : fd_BA_I2C est utilise
								unsigned char uc_BANumber,		// [IN] BA number (it's not then slave number on i2c...)
								unsigned char *p_uc_buffer,		// [IN] The array of characters are to be written in slave
								unsigned short us_n,			// [IN] Number of bytes in that buffer
								unsigned char *p_uc_Answer,		// [OUT] Value received in answer from I2C slave
								unsigned char *p_uc_CommStat	// [OUT] 6e octet de la reponse (comm_stat d'un BA 1.8, 0x55 pour un BA 1.7)
								)
{
	MQX_FILE_PTR l_fd;
	signed long l_sl_code_result;
	unsigned long l_ul_param;
	int l_i_ok;
	unsigned short l_us_length;	
	unsigned char l_uc_mem[1];
	signed long l_sl_result;
	unsigned char l_uc_code_trame;
	unsigned short l_us_CRC;
	unsigned short l_us_CRCRecu;
	unsigned char l_uc_CompteurEssaisReinit;
	unsigned char l_uc_ReinitOK;
	unsigned short l_us_CRCEnvoye;
	
	
	uc_Espionsl_fct_write_polled = 1;
	l_sl_code_result = I2C_OK;
	
	if(uc_DownloadEnCoursBloquerI2CVersBA == 0)	// Pas de dialogue I2C pendant telechargement
	{
		// Protect I2C transaction in multitask environment
		_lwsem_wait (&lock_I2C);
		uc_Espionsl_fct_write_polled = 2;
	
		
		l_us_CRCEnvoye = (p_uc_buffer[us_n-1]<<8) + p_uc_buffer[us_n-2];	// CRC envoyé : le dernier octet des donnees à envoyer
		
		// Reinitialisation complete de l'I2C demandé (suite plantage du a une perturbation)
		l_uc_ReinitOK = 1;
		if(uc_FlagReinitialiserI2C != 0)
		{
			l_uc_CompteurEssaisReinit = 0;
			l_uc_ReinitOK = 0;
			
			vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"!!!!!!!!!!!!!!!! REINIT START !!!!!!!!!!!!!!!!!\n");
			uc_Espionsl_fct_write_polled = 100;
			
			while(l_uc_ReinitOK == 0 && l_uc_CompteurEssaisReinit < uc_NB_REINIT_MAX)
			{
				vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\tEssai %d\n", l_uc_CompteurEssaisReinit+1);
	
				uc_Espionsl_fct_write_polled = 101;
				// 5.3 : un seul descripteur vivant ; si fclose echoue, le
				// descripteur courant est conserve et reconfigure.
				fd_BA_I2C = (MQX_FILE_PTR)bar_i2c_reopen(&s_st_I2C_Ops, (void *)fd_BA_I2C, &l_i_ok);
				uc_Espionsl_fct_write_polled = 106;
				if(l_i_ok != 0)
				{
					l_uc_ReinitOK = 1;
					uc_FlagReinitialiserI2C = 0;
				}
				uc_Espionsl_fct_write_polled = 107;
				l_uc_CompteurEssaisReinit++;
				vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\tEssai FIN %d -> %d\n", l_uc_CompteurEssaisReinit, l_uc_ReinitOK);
				if(l_uc_ReinitOK == 0)
				{
					vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"\ttempo\n");
					_time_delay(10);
				}
			}
			vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ERREUR,"!!!!!!!!!!!!!!!! REINIT FIN %d -> %d !!!!!!!!!!!!!!!!!\n", l_uc_CompteurEssaisReinit, l_uc_ReinitOK);
			uc_Espionsl_fct_write_polled = 108;
			
			if(l_uc_ReinitOK != 0)
			{
				if(l_uc_CompteurEssaisReinit > uc_NbEssaisReinitMax)	uc_NbEssaisReinitMax = l_uc_CompteurEssaisReinit;
			}
			else
			{
				uc_NbLoupesReinit++;
				l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_REINIT;
			}
		}
		
		lwgpio_set_value(&IO_DOUT_DebugJ1, LWGPIO_VALUE_HIGH);
		
		// 5.3 : toujours le descripteur en service (jamais le parametre, perime
		// apres une reinitialisation)
		(void)fd;
		l_fd = fd_BA_I2C;
		if(l_fd == NULL && l_sl_code_result == I2C_OK)
		{
			l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_REINIT;
		}

		// 5.1 : transaction bornee a 20 ms (i2c_bounded.h)
		i2cb_txn_begin();

		*p_uc_Answer = 0;
		*p_uc_CommStat = 0;
		uc_FlagBlocageDetecte = 0;
		
		uc_Espionsl_fct_write_polled = 3;
		
		//do // xxx ne sert à rien ? enlever !
		//{
			uc_Espionsl_fct_write_polled = 4;
			if(l_sl_code_result == I2C_OK)
			{
				uc_Espionsl_fct_write_polled = 5;
				l_ul_param = I2C_BUS_ADDRESS_BA + uc_BANumber;	// I2C bus address also contains memory block index
				uc_Espionsl_fct_write_polled = 6;
				l_sl_code_result = ioctl(l_fd, IO_IOCTL_I2C_SET_DESTINATION_ADDRESS, &l_ul_param);
				if(uc_FlagBlocageDetecte != 0)
				{
					l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_BLOCAGE;
					DETECTION_ERREUR_DIALOGUE_BA_SET_DESTINATION_ADDRESS(uc_BANumber);
					vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Blocage parametrage adresse esclave BA %i.%i\n", uc_BANumber, l_sl_code_result);
					printf("xxxxxx blocage 1 -> 0x%X\n", uc_FlagBlocageDetecte);
				}
				else
				{
					uc_Espionsl_fct_write_polled = 7;
					if(l_sl_code_result != I2C_OK)
					{
						uc_Espionsl_fct_write_polled = 8;
						DETECTION_ERREUR_DIALOGUE_BA_SET_DESTINATION_ADDRESS(uc_BANumber);
						vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur parametrage adresse esclave BA %i.%i\n", uc_BANumber, l_sl_code_result);
						uc_Espionsl_fct_write_polled = 9;
					}
				}
			}
			VERIF_BUDGET_TRANSACTION();
			uc_Espionsl_fct_write_polled = 10;
			if(l_sl_code_result == I2C_OK)
			{
				uc_Espionsl_fct_write_polled = 11;
				l_uc_mem[0] = 0;	// Donnée fictive non envoyée
				uc_Espionsl_fct_write_polled = 12;
				l_sl_result = fwrite(l_uc_mem, 1, 0, l_fd);	// Initiate start and send I2C bus address
				if(uc_FlagBlocageDetecte != 0)
				{
					l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_BLOCAGE;
					DETECTION_ERREUR_DIALOGUE_BA_EMISSION_DATA(uc_BANumber);
					vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Blocage emission address BA %i.%i\n", uc_BANumber, l_sl_result);
					printf("xxxxxx blocage 2 -> 0x%X\n", uc_FlagBlocageDetecte);
				}
				else
				{
					uc_Espionsl_fct_write_polled = 13;
					if(l_sl_result != 0)
					{
						uc_Espionsl_fct_write_polled = 131;
						DETECTION_ERREUR_DIALOGUE_BA_EMISSION_DATA(uc_BANumber);
						vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur emission address BA %i.%i\n", uc_BANumber, l_sl_result);
						l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_EMISSION_DATA;
						uc_Espionsl_fct_write_polled = 132;
					}
				}
			}
	
			VERIF_BUDGET_TRANSACTION();
			if(l_sl_code_result == I2C_OK)
			{
				l_sl_code_result = ioctl(l_fd, IO_IOCTL_FLUSH_OUTPUT, &l_ul_param); // Check ack (device exists)
				if(uc_FlagBlocageDetecte != 0)
				{
					l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_BLOCAGE;
					DETECTION_ERREUR_DIALOGUE_BA_START(uc_BANumber);
					vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Blocage generation Start BA %i.%i\n", uc_BANumber, l_sl_code_result);
					printf("xxxxxx blocage 3 -> 0x%X\n", uc_FlagBlocageDetecte);
				}
				else
				{
					uc_Espionsl_fct_write_polled = 14;
					if(l_sl_code_result == I2C_OK)
					{
						uc_Espionsl_fct_write_polled = 15;
						if(l_ul_param != 0) 	// fin de trame demandée
						{
							uc_Espionsl_fct_write_polled = 16;
							DETECTION_ERREUR_DIALOGUE_BA_CHK(uc_BANumber);
							vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur acquit. Start BA %i.%i\n", uc_BANumber, l_ul_param);
							l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_FIN_TRAME;
							uc_Espionsl_fct_write_polled = 17;
						}
						uc_Espionsl_fct_write_polled = 18;
					}
					else
					{
						uc_Espionsl_fct_write_polled = 19;
						DETECTION_ERREUR_DIALOGUE_BA_START(uc_BANumber);
						vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur generation Start BA %i.%i\n", uc_BANumber, l_sl_code_result);
						uc_Espionsl_fct_write_polled = 20;
					}
				}
			}
			VERIF_BUDGET_TRANSACTION();
			uc_Espionsl_fct_write_polled = 21;
			if(l_sl_code_result == I2C_OK)
			{
				uc_Espionsl_fct_write_polled = 22;
				l_us_length = us_n;
				uc_Espionsl_fct_write_polled = 23;
				l_sl_result = fwrite(p_uc_buffer, 1, l_us_length, l_fd);	// Page write of data
				if(uc_FlagBlocageDetecte != 0)
				{
					l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_BLOCAGE;
					DETECTION_ERREUR_DIALOGUE_BA_EMISSION_DATA(uc_BANumber);
					vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Blocage nombre octets emis BA %i.%i\n", uc_BANumber, l_sl_result);
					printf("xxxxxx blocage 4 -> 0x%X\n", uc_FlagBlocageDetecte);
				}
				else
				{
					uc_Espionsl_fct_write_polled = 24;
					if(l_sl_result != l_us_length)
					{
						uc_Espionsl_fct_write_polled = 25;
						DETECTION_ERREUR_DIALOGUE_BA_EMISSION_DATA(uc_BANumber);
						vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur nombre octets emis BA %i.%i\n", uc_BANumber, l_sl_result);
						l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_EMISSION_DATA;
						uc_Espionsl_fct_write_polled = 26;
					}
				}
			}
			VERIF_BUDGET_TRANSACTION();
			uc_Espionsl_fct_write_polled = 27;
			if(l_sl_code_result == I2C_OK)
			{
				uc_Espionsl_fct_write_polled = 28;
				l_sl_code_result = fflush (l_fd);	//Wait for completion
				if(uc_FlagBlocageDetecte != 0)
				{
					l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_BLOCAGE;
					DETECTION_ERREUR_DIALOGUE_BA_EMISSION_COMPLETE(uc_BANumber);
					vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Blocage attente emission complete BA %i.%i\n", uc_BANumber, l_sl_code_result);
					printf("xxxxxx blocage 5 -> 0x%X\n", uc_FlagBlocageDetecte);
				}
				else
				{
					uc_Espionsl_fct_write_polled = 29;
					if(l_sl_code_result != MQX_OK)
					{
						uc_Espionsl_fct_write_polled = 30;
						DETECTION_ERREUR_DIALOGUE_BA_EMISSION_COMPLETE(uc_BANumber);
						vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur attente emission complete BA %i.%i\n", uc_BANumber, l_sl_code_result);
						uc_Espionsl_fct_write_polled = 31;
					}
				}
			}
			VERIF_BUDGET_TRANSACTION();
			uc_Espionsl_fct_write_polled = 32;
			if(l_sl_code_result == I2C_OK)
			{
				uc_Espionsl_fct_write_polled = 33;
				l_sl_code_result = ioctl(l_fd, IO_IOCTL_I2C_REPEATED_START, NULL);	// Restart I2C transfer for reading
				if(uc_FlagBlocageDetecte != 0)
				{
					l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_BLOCAGE;
					DETECTION_ERREUR_DIALOGUE_BA_EMISSION_RESTART(uc_BANumber);
					vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Blocage sur emission du ReStart BA %i.%i\n", uc_BANumber, l_sl_code_result);
					printf("xxxxxx blocage 6 -> 0x%X\n", uc_FlagBlocageDetecte);
				}
				else
				{
					uc_Espionsl_fct_write_polled = 34;
					if(l_sl_code_result != I2C_OK)
					{
						uc_Espionsl_fct_write_polled = 35;
						DETECTION_ERREUR_DIALOGUE_BA_EMISSION_RESTART(uc_BANumber);
						vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur sur emission du ReStart BA %i.%i\n", uc_BANumber, l_sl_code_result);
						uc_Espionsl_fct_write_polled = 36;
					}
				}
			}
			VERIF_BUDGET_TRANSACTION();
			uc_Espionsl_fct_write_polled = 37;
			if(l_sl_code_result == I2C_OK)
			{
				uc_Espionsl_fct_write_polled = 38;
				l_ul_param = uc_I2C_NB_OCTETS_REPONSE;	// 5.5 : 6 octets (comm_stat d'un BA 1.8 ; 0x55 pour un BA 1.7)
				uc_Espionsl_fct_write_polled = 39;
				l_sl_code_result = ioctl (l_fd, IO_IOCTL_I2C_SET_RX_REQUEST, &l_ul_param);	// Set read request
				if(uc_FlagBlocageDetecte != 0)
				{
					l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_BLOCAGE;
					DETECTION_ERREUR_DIALOGUE_BA_EMISSION_RESTART(uc_BANumber);
					vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Blocage sur demande de lecture BA %i.%i\n", uc_BANumber, l_sl_code_result);
					printf("xxxxxx blocage 7 -> 0x%X\n", uc_FlagBlocageDetecte);
				}
				else
				{
					uc_Espionsl_fct_write_polled = 40;
					if(l_sl_code_result != I2C_OK)
					{
						uc_Espionsl_fct_write_polled = 41;
						DETECTION_ERREUR_DIALOGUE_BA_EMISSION_RESTART(uc_BANumber);
						vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur sur demande de lecture BA %i.%i\n", uc_BANumber, l_sl_code_result);
						uc_Espionsl_fct_write_polled = 42;
					}
				}
			}
			VERIF_BUDGET_TRANSACTION();
			uc_Espionsl_fct_write_polled = 43;
			if(l_sl_code_result == I2C_OK)
			{
				uc_Espionsl_fct_write_polled = 44;
				l_uc_code_trame = p_uc_buffer[0];
				uc_Espionsl_fct_write_polled = 45;
				us_n = uc_I2C_NB_OCTETS_REPONSE;
				l_sl_code_result = fread(p_uc_buffer, 1, us_n, l_fd);	// Read all data
				if(uc_FlagBlocageDetecte != 0)
				{
					l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_BLOCAGE;
					DETECTION_ERREUR_DIALOGUE_BA_TAILLE_REPONSE(uc_BANumber);
					vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Blocage nombre d octets recus BA %i.%i\n", uc_BANumber, l_sl_code_result);
					printf("xxxxxx blocage 8 -> 0x%X\n", uc_FlagBlocageDetecte);
				}
				else
				{
					uc_Espionsl_fct_write_polled = 46;
					if(l_sl_code_result == uc_I2C_NB_OCTETS_REPONSE)
					{
						vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ACTIVITE,"BA : réception -> %d.%d.%d.%d.%d\n", p_uc_buffer[0], p_uc_buffer[1], p_uc_buffer[2], p_uc_buffer[3], p_uc_buffer[4]);
						uc_Espionsl_fct_write_polled = 47;
						// vérification du code de trame : doit être le même que celui qui a été émis
						if(l_uc_code_trame == p_uc_buffer[0])
						{
							uc_Espionsl_fct_write_polled = 48;
							*p_uc_Answer = p_uc_buffer[1]; 
		
							// vérification de la cheksum
							uc_Espionsl_fct_write_polled = 49;
							//l_uc_CheckSum = p_uc_buffer[0]+p_uc_buffer[1]+p_uc_buffer[2];
							l_us_CRC = us_CalculerCRCSurTrame(p_uc_buffer, 3);
							vd_EspionRS_Printf(uc_ESPION_TACHE_BA_ACTIVITE,"BA : CRC calculé -> %d\n", l_us_CRC);
							uc_Espionsl_fct_write_polled = 50;
							//if(l_uc_CheckSum != 0)
							l_us_CRCRecu = p_uc_buffer[3] + (p_uc_buffer[4]<<8);
							if(l_us_CRC != l_us_CRCRecu)
							{
								uc_Espionsl_fct_write_polled = 51;
								// erreur cheksum
								// xxx à tester
								DETECTION_ERREUR_DIALOGUE_BA_CRC_REPONSE(uc_BANumber);
								vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur cheksum recue BA %i.%i\n", uc_BANumber, l_us_CRCRecu, l_us_CRC);
								l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_CRC_REPONSE;
								uc_Espionsl_fct_write_polled = 52;
							}
							else
							{
								// Check CRC envoye == CRC retourné
								l_us_CRCRecu = p_uc_buffer[1] + (p_uc_buffer[2]<<8);
								if(l_us_CRCEnvoye != l_us_CRCRecu)
								{
									uc_Espionsl_fct_write_polled = 53;
									// erreur cheksum
									// xxx à tester
									DETECTION_ERREUR_DIALOGUE_BA_CRC_RETOUR(uc_BANumber);
									vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur cheksum renvoye BA %i.%i != %i envoye\n", uc_BANumber, l_us_CRCRecu, l_us_CRCEnvoye);
									l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_CRC_REPONSE;
									uc_Espionsl_fct_write_polled = 54;
								}
								else
								{
									// Verification CRC envoye = CRC renvoye
									uc_Espionsl_fct_write_polled = 55;
									*p_uc_CommStat = p_uc_buffer[5];	// hors CRC : valide par sa signature (ba_resil)
									// Checksum OK
									l_sl_code_result = I2C_OK;
									uc_Espionsl_fct_write_polled = 56;
								}
							}
						}
						else
						{
							uc_Espionsl_fct_write_polled = 57;
							DETECTION_ERREUR_DIALOGUE_BA_CODE_REPONSE(uc_BANumber);
							vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur code Reponse BA %i.%i.%i\n", uc_BANumber, l_uc_code_trame, p_uc_buffer[0]);
							l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_CODE_REPONSE;
							uc_Espionsl_fct_write_polled = 58;
						}
					}
					else
					{
						uc_Espionsl_fct_write_polled = 59;
						DETECTION_ERREUR_DIALOGUE_BA_TAILLE_REPONSE(uc_BANumber);
						vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur nombre d octets recus BA %i.%i\n", uc_BANumber, l_sl_code_result);
						l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_TAILLE_RECEPTION;
						uc_Espionsl_fct_write_polled = 60;
					}
				}
			}
			uc_Espionsl_fct_write_polled = 61;
			//us_n -= l_us_length;	// Next round
	
		//}while(us_n);
	
		// Stop I2C transfer 
		uc_Espionsl_fct_write_polled = 70;
		// A faire dans tous les cas sauf si pb reinit
		if(l_uc_ReinitOK != 0)
		{
			l_sl_result = ioctl(l_fd, IO_IOCTL_I2C_STOP, NULL);
			if(uc_FlagBlocageDetecte != 0)
			{
				l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_BLOCAGE;
				DETECTION_ERREUR_DIALOGUE_BA_STOP_TRANSFERT(uc_BANumber);
				vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Blocage emission du Stop BA %i.%i\n", uc_BANumber, l_sl_result);
				printf("xxxxxx blocage 9 -> 0x%X\n", uc_FlagBlocageDetecte);
			}
			else
			{
				uc_Espionsl_fct_write_polled = 71;
				if(l_sl_result != I2C_OK)
				{
					uc_Espionsl_fct_write_polled = 72;
					DETECTION_ERREUR_DIALOGUE_BA_STOP_TRANSFERT(uc_BANumber);
					vd_EspionRS_Printf(uc_ESPION_DIALOGUE_BA_ERREUR,"BA : Erreur emission du Stop BA %i.%i\n", uc_BANumber, l_sl_result);
					uc_Espionsl_fct_write_polled = 73;
					if(l_sl_code_result == I2C_OK)
					{
						uc_Espionsl_fct_write_polled = 74;
						l_sl_code_result = l_sl_result;
						uc_Espionsl_fct_write_polled = 75;
					}
				}
			}
		}
	
		uc_Espionsl_fct_write_polled = 76;
		i2cb_txn_end();
		_lwsem_post (&lock_I2C);	//Release the transaction lock
	
		lwgpio_set_value(&IO_DOUT_DebugJ1, LWGPIO_VALUE_LOW);
		
		uc_Espionsl_fct_write_polled = 77;
		
		if(uc_FlagBlocageDetecte != 0)
		{
			printf("xxxxxxxxxxxxxxxxxxxxx blocage i2c détecté %d -> demande réinit au prochain envoi\n", uc_FlagBlocageDetecte);
			uc_FlagReinitialiserI2C = 1;
			uc_FlagBlocageDetecte = 0;
			ul_EspionTacheBANbBlocageI2C++;
			l_sl_code_result = sl_FCT_WRITE_POLLED_ERREUR_BLOCAGE;
		}
		
		// Tempo dans tous les cas pour laisser aux BA le temps de traiter la trame envoyée
		// Si erreur, on fait quand même la tempo...
		_time_delay(100);		// BA : traitement toutes les 40 ms...
	}
	
	return (l_sl_code_result);  
}
