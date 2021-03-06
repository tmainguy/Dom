#include <ethernet_comp.h>
#include <UIPEthernet.h>

/**********************************************************************
*                        definition des variables                     *
**********************************************************************/

// N° de port du serveur
EthernetServer server = EthernetServer(1000);

// Nb total de volet
#define Nb_Volet 6

// Nb total de inter
#define Nb_Inter 8   // 1 inter par volet + l'inter Gen n° Nb_Volet + 1


// structure volet
typedef struct
{
  byte InterMon;
  byte InterMonAv;
  byte InterDes;
  byte InterDesAv;
  byte Cmd;         // 0 => rien / 1=> descente / 2 => arrêt / 3 => montée
  byte PortInterMon;
  byte PortInterDes;
}  inter_type;

inter_type inter[Nb_Inter];

// structure volet
typedef struct
{
  int Etat;        // Etat des volet : -1 => descente / 0 => arrêt / 1 montée
  byte Position;    // 0 : bas / 100 : haut
  unsigned long  TempsMontee; // en ms
  unsigned long StopTime;    // temps d'arrêt
  byte PortVolMon;
  byte PortVolDes;
}  volet_type;

volet_type volet[Nb_Volet];

// Quelques variables
int i, boucle, PremierPassage, prem;
String str_reponse;
char type_requete[4];

//**********************************************************************
//                             Initialisation                          *
//**********************************************************************

void setup()
{
  // initialisation des inter
  inter[0] = (inter_type) {
    0, 0, 0, 0, 0, 24, 25
  };
  inter[1] = (inter_type) {
    0, 0, 0, 0, 0, 26, 27
  };
  inter[2] = (inter_type) {
    0, 0, 0, 0, 0, 28, 29
  };
  inter[3] = (inter_type) {
    0, 0, 0, 0, 0, 30, 31
  };
  inter[4] = (inter_type) {
    0, 0, 0, 0, 0, 32, 33
  };
  inter[5] = (inter_type) {
    0, 0, 0, 0, 0, 34, 35
  };
  inter[6] = (inter_type) {
    0, 0, 0, 0, 0, 36, 37
  };
  inter[7] = (inter_type) {
    0, 0, 0, 0, 0, 22, 23
  };  // InterGene


  for (i = 0; i < Nb_Inter; i = i + 1)
  {
    pinMode(inter[i].PortInterMon, OUTPUT);
    digitalWrite(inter[i].PortInterMon, HIGH);          // pull Up
    pinMode(inter[i].PortInterDes, OUTPUT);
    digitalWrite(inter[i].PortInterDes, HIGH);          // pull Up
  }

  // initialisation des relais
  volet[0] = (volet_type) {
    0, 0, 10000, 0, 38, 39
  };
  volet[1] = (volet_type) {
    0, 0, 22000, 0, 40, 41
  };
  volet[2] = (volet_type) {
    0, 0, 22000, 0, 42, 43
  };
  volet[3] = (volet_type) {
    0, 0, 22000, 0, 44, 45
  };
  volet[4] = (volet_type) {
    0, 0, 22000, 0, 46, 47
  };
  volet[5] = (volet_type) {
    0, 0, 22000, 0, 48, 49
  };
  //volet[6] = (volet_type) { 0, 0, 10000, 0, 2, 3 };

  // Configuration des ports des relais comme sortie numerique.
  for (i = 0; i < Nb_Volet; i = i + 1)
  {
    pinMode(volet[i].PortVolMon, OUTPUT);
    digitalWrite(volet[i].PortVolMon, HIGH);
    pinMode(volet[i].PortVolDes, OUTPUT);
    digitalWrite(volet[i].PortVolDes, HIGH);
  }

  Serial.begin(9600);      // open the serial port at 9600 bps:
  boucle = 0;
  PremierPassage = 1;

  uint8_t mac[6] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
  IPAddress myIP(192, 168, 0, 6);

  Ethernet.begin(mac, myIP);
  server.begin();

}

//**********************************************************************
//                             Programme principal                     *

//**********************************************************************

void loop()

{
  size_t size;
  int int_Numvolet, int_Cmd;
  
  //String str(Message);

  // Première lecture des états des interrupteurs
  if ( PremierPassage == 1)
  {
    Premiere_lecture();
    PremierPassage = 0;
  }

  // Lecture des états des interrupteurs
  Lecture_Inter();

  // Calcul des commandes interrupteurs
  Calcul_commandes_inter();

  // on regarde ce qui se passe sur le réseau avant de calculer les commandes
  if (EthernetClient client = server.available())
  {
    while ((size = client.available()) > 0)
    {
      uint8_t* msg = (uint8_t*)malloc(size);
      size = client.read(msg, size);
      Serial.write(msg, size);
      // recupération du début de la requete
      for (i = 0; i < 4; i++)
      {
        type_requete[i] = msg[i];
        //  Serial.println(Message[i]);
      }
      free(msg);
      
      // on regarde si la requete est un GET
      if ( strncmp(type_requete, "GET", 3) == 0)
      {
        str_reponse = "Arduino1:";
        for (i = 0; i < Nb_Volet; i = i + 1)
        {
          str_reponse += "volet";
          str_reponse += i + 1;
          str_reponse += "|";
          str_reponse += volet[i].Etat;
          str_reponse += "|";
          str_reponse += volet[i].Position;
          str_reponse += "|";
        }
        client.println(str_reponse);
      }
      // on regarde si la requete est un post
      else if ( strncmp(type_requete, "POST", 4) == 0)
      {
        // dans ce cas : on recupére la fin du message du type "name=Vx|y"
        // x : num de volet =>0-6 : Volet / 7 : tous
        // y : cmd => 0 => rien / 1=> descente / 2 => arrêt / 3 => montée
        int_Numvolet = msg[size-3] - '0';
        int_Cmd = msg[size-1] - '0';
        if ( int_Numvolet <= Nb_Volet && int_Numvolet > 0)  // commande sur 1 volet
        {
          inter[int_Numvolet - 1].Cmd = int_Cmd - 1;
        }
        else if ( int_Numvolet == 0)                        // commande gene
        {
           inter[Nb_Inter - 1].Cmd = int_Cmd;
        }
        str_reponse = "Arduino1:POST: ";
        str_reponse += int_Numvolet;
        str_reponse += ":cmd";
        str_reponse += int_Cmd;
        client.println(str_reponse);
      }
    }
    client.stop();
  }

  // Puis Pilotage des relais des volets pour montée / descente / arrêt
  Gestion_relais();

  // Gestion des tempo pour arrêter les volets en fin de course
  Gestion_tempo();

  // petite pause : à supprimer par la suite
  //delay(100);
}

//**********************************************************************
//                             Fonction gestion des tempos             *
//            => arrêt des volets en mouvement                         *
//**********************************************************************

void Gestion_tempo ()
{
  for (i = 0; i < Nb_Volet; i = i + 1)
  {
    if (volet[i].Etat != 0 && (long) (millis() - volet[i].StopTime) > 0)
    {
      Arret_Volet (i);
    }
  }
}

//**********************************************************************
//                             Fonction pilotage des relais            *
//**********************************************************************

void Gestion_relais ()
{
  switch (inter[Nb_Inter - 1].Cmd) {            // On commence par regarder si il y a une commande Gen
    case 1:                                     //descente Gene
      //Serial.println("case 1");
      for (i = 0; i < Nb_Volet; i = i + 1)
      {
        Descente_Volet(i, 100);
        inter[Nb_Volet + 1].Cmd = 0;
      }
      break;
    case 2:                                     //arrêt Gene
      //Serial.println("case 2");
      for (i = 0; i < Nb_Volet; i = i + 1)
      {
        Arret_Volet (i);
        inter[Nb_Volet + 1].Cmd = 0;
      }
      break;
    case 3:                                     //Montée Gene
      //Serial.println("case 3");
      for (i = 0; i < Nb_Volet; i = i + 1)
      {
        Montee_Volet(i, 100);
        inter[Nb_Volet + 1].Cmd = 0;
      }
      break;
    default:                                    // Pas de commande Gene : on traite les volets au cas par cas
      for (i = 0; i < Nb_Volet; i = i + 1)
      {
        switch (inter[i].Cmd) {
          case 1:                              //descente
            Descente_Volet(i, 100);
            break;
          case 2:                              // arrêt
            Arret_Volet (i);
            break;
          case 3:			       //Montée
            Montee_Volet(i, 100);
            break;
        }
      }
  }
}

/****************************************************************
*               fonction de montee du volet
* Param :
*        N° du volet à monter
*        hauteur du volet (0 : bas / 100 : haut)  => pas utilisé
****************************************************************/
void Montee_Volet ( int i_Num_Volet, int niveau)
{
  //Serial.print("*** Montee volet n ");
  //Serial.println(i + 1);
  digitalWrite(volet[i_Num_Volet].PortVolMon, LOW);
  digitalWrite(volet[i_Num_Volet].PortVolDes, HIGH);
  volet[i_Num_Volet].StopTime = millis() + volet[i_Num_Volet].TempsMontee;                     // calcul "heure" d'arrêt du volet
  volet[i_Num_Volet].Etat = 1;
}

/****************************************************************
*            fonction de descente du volet
* Param :
*        N° du volet à descendre
*        hauteur du volet (0 : bas / 100 : haut) => pas utilisé
****************************************************************/
void Descente_Volet (int i_Num_Volet, int niveau)
{
  //Serial.print("*** Descente volet n ");
  //Serial.println(i + 1);
  digitalWrite(volet[i_Num_Volet].PortVolMon, HIGH);
  digitalWrite(volet[i_Num_Volet].PortVolDes, LOW);
  volet[i_Num_Volet].StopTime = millis() + volet[i_Num_Volet].TempsMontee;                    // calcul "heure" d'arrêt du volet
  volet[i_Num_Volet].Etat = -1;
}
/****************************************************************
*               fonction d'arrêt du volet
* Param :
*        N° du volet à arrêter
***************************************************************/
void Arret_Volet (int i_Num_Volet)
{
  //Serial.print("*** Arret volet n ");
  //Serial.println(i + 1);
  digitalWrite(volet[i_Num_Volet].PortVolMon, HIGH);
  digitalWrite(volet[i_Num_Volet].PortVolDes, HIGH);
  volet[i_Num_Volet].Etat = 0;
}


//**********************************************************************
//                             Fonction premiere lecture               *
//**********************************************************************

void Premiere_lecture()
{
  for (i = 0; i < Nb_Inter; i = i + 1)
  {
    inter[i].InterMon = digitalRead(inter[i].PortInterMon);
    inter[i].InterDes = digitalRead(inter[i].PortInterDes);
  }
}


//**********************************************************************
//                             Fonction lecture                    *
//**********************************************************************

void Lecture_Inter ()
{
  for (i = 0; i < Nb_Inter ; i = i + 1)
  {
    inter[i].InterMonAv =  inter[i].InterMon;
    inter[i].InterDesAv = inter[i].InterDes;
    inter[i].InterMon = digitalRead(inter[i].PortInterMon);
    inter[i].InterDes = digitalRead(inter[i].PortInterDes);
  }
}

//**********************************************************************
//                             Fonction calcul commande                *
//**********************************************************************
void Calcul_commandes_inter ()
{
  for (i = 0; i < Nb_Inter ; i = i + 1)
  {
    inter[i].Cmd =  Calcul_commande (inter[i].InterMonAv, inter[i].InterMon, inter[i].InterDesAv, inter[i].InterDes);
    /*    if (inter[i].Cmd != 0)
        {
          affichage = "Calcul_commandes_inter ";
          affichage += i;
          affichage += " : commande ";
          affichage += inter[i].Cmd;
          Serial.println(affichage);
        }*/
  }

}
//**********************************************************************
//                             Fonction calcul commande                *
//**********************************************************************
byte Calcul_commande (byte AvM, byte M, byte AvD, byte D)
{
  byte commande = 0;
  if (M == 1 && AvM == 0) // demande de montée
  {
    commande = 3;
  }
  else if ( D == 1 && AvD == 0 ) // demande de descente
  {
    commande = 1;
  }
  else if ( AvM > M || AvD > D ) // demande d'arrêt
  {
    commande = 2;
  }
  return commande;
}

