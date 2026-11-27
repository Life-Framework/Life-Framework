StringTable {
 ItemClassName "CustomStringTableItem"
 Items {
  CustomStringTableItem "{1CB6BD5316FBA315}" {
   Id "EL-ATM_Amount"
   Target_en_us "Amount:"
   Target_es_es "Cantidad:"
   Target_de_de "Betrag:"
   Target_fr_fr "Montant :"
   Target_it_it "Importo :"
   Target_pt_pt "Quantidade:"
   Target_pt_br "Quantidade:"
  }
    CustomStringTableItem "{85E427AE19AEEF1C}" {
     Id "EL-ATM_AmountPlaceholder"
     Target_en_us "Enter amount"
     Target_de_de "Betrag eingeben"
     Target_fr_fr "Saisissez le montant"
     Target_it_it "Inserisci l'importo"
     Target_es_es "Introduce la cantidad"
     Target_pt_pt "Introduza o montante"
     Target_pt_br "Digite o valor"
     Comment "ATM deposit/withdraw amount field placeholder"
    }
  CustomStringTableItem "{E687B439A5205BE9}" {
   Id "EL-ATM_Balance"
   Target_en_us "Balance: $"
   Target_es_es "Saldo: $"
   Target_de_de "Kontostand: $"
   Target_fr_fr "Solde : $"
   Target_it_it "Saldo : $"
   Target_pt_pt "Saldo: $"
   Target_pt_br "Saldo: $"
  }
    CustomStringTableItem "{4B31E1642B5E97C0}" {
     Id "EL-ATM_Close"
     Target_en_us "Close"
     Comment "Button that closes the ATM screen"
    }
  CustomStringTableItem "{0379A5BDF0064A16}" {
   Id "EL-ATM_Deposit"
   Target_en_us "Deposit"
   Target_es_es "Depositar"
   Target_de_de "Einzahlen"
   Target_fr_fr "Déposer"
   Target_it_it "Deposita"
   Target_pt_pt "Depositar"
   Target_pt_br "Depositar"
  }
    CustomStringTableItem "{16D56900DC5246BC}" {
     Id "EL-ATM_DepositRefused"
     Target_en_us "The teller refused the deposit: you do not carry that much cash"
     Comment "ATM deposit refused by the server"
    }
  CustomStringTableItem "{E9F2EDAF4646211E}" {
   Id "EL-ATM_InsufficientFunds"
   Target_en_us "Insufficient funds"
   Target_es_es "Fondos insuficientes"
   Target_de_de "Unzureichende Mittel"
   Target_fr_fr "Fonds insuffisants"
   Target_it_it "Fondi insufficienti"
   Target_pt_pt "Fundos insuficientes"
   Target_pt_br "Fundos insuficientes"
  }
    CustomStringTableItem "{C1337327CA2B69B2}" {
     Id "EL-ATM_InvalidAmount"
     Target_en_us "Enter an amount greater than zero"
     Comment "ATM deposit/withdraw refused on the client because the typed amount is empty, zero or negative"
    }
  CustomStringTableItem "{B3BF4076847C9C6E}" {
   Id "EL-ATM_Title"
   Target_en_us "ATM"
   Target_es_es "Cajero Automático"
   Target_de_de "Geldautomat"
   Target_fr_fr "Distributeur automatique"
   Target_it_it "Bancomat"
   Target_pt_pt "Caixa automático"
   Target_pt_br "Caixa eletrônico"
  }
    CustomStringTableItem "{B0D42569EC54BD93}" {
     Id "EL-ATM_TooFar"
     Target_en_us "Stand at an ATM to do that"
     Comment "ATM deposit or withdraw refused because the player walked away from the ATM"
    }
  CustomStringTableItem "{C726A3F43803900E}" {
   Id "EL-ATM_Withdraw"
   Target_en_us "Withdraw"
   Target_es_es "Retirar"
   Target_de_de "Abheben"
   Target_fr_fr "Retirer"
   Target_it_it "Preleva"
   Target_pt_pt "Levantar"
   Target_pt_br "Sacar"
  }
    CustomStringTableItem "{5FF37114605F0B03}" {
     Id "EL-ATM_WithdrawRefused"
     Target_en_us "The teller refused the withdrawal: the account does not hold that much"
     Comment "ATM withdrawal refused by the server"
    }
    CustomStringTableItem "{B7E41C90A2D64F13}" {
     Id "EL-Bank_CiviliansOnly"
     Target_en_us "Only civilians can rob the vault"
     Comment "Bank vault robbery refused for a non-civilian"
    }
    CustomStringTableItem "{B7E41C90A2D64F12}" {
     Id "EL-Bank_Cooldown"
     Target_en_us "The vault is empty"
     Comment "Bank vault robbery refused because the cooldown has not elapsed"
    }
    CustomStringTableItem "{B7E41C90A2D64F14}" {
     Id "EL-Bank_NeedPolice"
     Target_en_us "Not enough {police} on duty"
     Comment "Bank vault robbery refused below the minimum police count"
    }
    CustomStringTableItem "{B7E41C90A2D64F15}" {
     Id "EL-Bank_NotReady"
     Target_en_us "This vault cannot be robbed"
     Comment "Bank vault robbery refused because the vault or the player is not ready"
    }
    CustomStringTableItem "{B7E41C90A2D64F11}" {
     Id "EL-Bank_Rob"
     Target_en_us "Breach the vault"
     Comment "Bank vault robbery action name"
    }
    CustomStringTableItem "{F4D6F78E634D1216}" {
     Id "EL-Benefit_Collect"
     Target_en_us "Collect benefit"
     Comment "Job counter action: collect the unemployment benefit in cash"
    }
    CustomStringTableItem "{13CD00AAEFCF43FD}" {
     Id "EL-Benefit_Paid"
     Target_en_us "Unemployment benefit: {currency:sym}%1 cash"
     Comment "Toast after the job counter hands over the unemployment benefit; %1 is the amount"
    }
    CustomStringTableItem "{6B1E0C2D9A4F7E31}" {
     Id "EL-Benefit_Refused_Balance"
     Target_en_us "You have too much money for the benefit"
     Comment "Benefit refused because the bank balance plus carried cash is at or above the threshold"
    }
    CustomStringTableItem "{6B1E0C2D9A4F7E33}" {
     Id "EL-Benefit_Refused_Cooldown"
     Target_en_us "You collected the benefit recently. Come back later."
     Comment "Benefit refused because the cooldown since the last collection has not elapsed"
    }
    CustomStringTableItem "{6B1E0C2D9A4F7E34}" {
     Id "EL-Benefit_Refused_Disabled"
     Target_en_us "The benefit is not paid here"
     Comment "Benefit refused because it is disabled in config or the player has no account"
    }
    CustomStringTableItem "{6B1E0C2D9A4F7E32}" {
     Id "EL-Benefit_Refused_Working"
     Target_en_us "The benefit is for people out of work"
     Comment "Benefit refused because the player is on duty as police or has an active delivery run"
    }
    CustomStringTableItem "{20909EFABF36AC7A}" {
     Id "EL-Benefit_Title"
     Target_en_us "Job counter"
     Comment "Toast title for the unemployment benefit"
    }
    CustomStringTableItem "{D399AB029024C8FF}" {
     Id "EL-Breach_AlreadyOpen"
     Target_en_us "A charge is already on this"
     Comment "Breach arm refused because the target is open or already armed"
    }
    CustomStringTableItem "{6CA7861D9153E199}" {
     Id "EL-Breach_Arm"
     Target_en_us "Place charge"
     Comment "Breach arm action name"
    }
    CustomStringTableItem "{6717E4FCF46F11F2}" {
     Id "EL-Breach_NeedCharge"
     Target_en_us "You need a demolition charge"
     Comment "Breach arm refused because the player carries no M112 demolition charge"
    }
    CustomStringTableItem "{E5160A26BE539A4D}" {
     Id "EL-Breach_NotConfigured"
     Target_en_us "This cannot be breached"
     Comment "Breach action refused because the target is not configured"
    }
    CustomStringTableItem "{A274291AAA4D0128}" {
     Id "EL-Breach_Repair"
     Target_en_us "Repair breach"
     Comment "Breach repair action name"
    }
    CustomStringTableItem "{A3FCAC46DE2FB463}" {
     Id "EL-Breach_RepairDenied"
     Target_en_us "Only {police} or the owner can repair this"
     Comment "Breach repair refused for this player"
    }
    CustomStringTableItem "{B3A464BFF4F596CC}" {
     Id "EL-CC_AgeHint"
     Target_en_us "18-99"
     Target_de_de "18-99"
     Target_fr_fr "18-99"
     Target_it_it "18-99"
     Target_es_es "18-99"
     Target_pt_pt "18-99"
     Target_pt_br "18-99"
     Comment "Character creation age input hint"
    }
    CustomStringTableItem "{A030E68CDF44FE1B}" {
     Id "EL-CC_Error_Age"
     Target_en_us "Enter an age between 18 and 80."
     Target_de_de "Gib ein Alter zwischen 18 und 80 ein."
     Target_fr_fr "Saisissez un âge entre 18 et 80."
     Target_it_it "Inserisci un'età tra 18 e 80."
     Target_es_es "Introduce una edad entre 18 y 80."
     Target_pt_pt "Introduz uma idade entre 18 e 80."
     Target_pt_br "Digite uma idade entre 18 e 80."
     Comment "Character creation invalid age error"
    }
    CustomStringTableItem "{1D9FBDE69C69275F}" {
     Id "EL-CC_Error_Exists"
     Target_en_us "This account already has a character."
     Comment "Character creation refused because the account already has a character"
    }
    CustomStringTableItem "{65A21AB01575E40D}" {
     Id "EL-CC_Error_FactionDenied"
     Target_en_us "You are not cleared for that faction."
     Comment "Faction pick refused by the server whitelist. The faction menu opens again."
    }
    CustomStringTableItem "{9EFAFBA759167819}" {
     Id "EL-CC_Error_FactionLocked"
     Target_en_us "Your faction is already set."
     Comment "Faction or character request refused because the account already chose a faction, or has not chosen one yet for a character"
    }
    CustomStringTableItem "{B52735364D1CAC50}" {
     Id "EL-CC_Error_Fields"
     Target_en_us "Enter a first and last name."
     Target_de_de "Gib einen Vor- und Nachnamen ein."
     Target_fr_fr "Saisissez un prénom et un nom."
     Target_it_it "Inserisci nome e cognome."
     Target_es_es "Introduce un nombre y un apellido."
     Target_pt_pt "Introduz um nome próprio e um apelido."
     Target_pt_br "Digite um nome e um sobrenome."
     Comment "Character creation empty name error"
    }
    CustomStringTableItem "{654A62843AE6EBC8}" {
     Id "EL-CC_Error_Location"
     Target_en_us "That spawn location is not available."
     Comment "Spawn location refused because it does not belong to the chosen faction. The location menu opens again."
    }
    CustomStringTableItem "{584BFA3C9B4E7978}" {
     Id "EL-CC_Error_NameLength"
     Target_en_us "Names can be at most 32 characters."
     Comment "Character creation refused because a name is longer than 32 characters"
    }
    CustomStringTableItem "{C84EED35CC0A568B}" {
     Id "EL-CC_Error_NoAccount"
     Target_en_us "Your account is still loading. Try again."
     Comment "Creation request refused because the server has no account for the player yet"
    }
    CustomStringTableItem "{BC676004A2DBE680}" {
     Id "EL-CC_FirstNamePlaceholder"
     Target_en_us "Enter your first name..."
     Target_de_de "Gib deinen Vornamen ein..."
     Target_fr_fr "Saisissez votre prénom..."
     Target_it_it "Inserisci il tuo nome..."
     Target_es_es "Introduce tu nombre..."
     Target_pt_pt "Introduza o seu primeiro nome..."
     Target_pt_br "Digite seu primeiro nome..."
     Comment "Character creation first name field placeholder"
    }
    CustomStringTableItem "{4477E169EA738896}" {
     Id "EL-CC_LastNamePlaceholder"
     Target_en_us "Enter your last name..."
     Target_de_de "Gib deinen Nachnamen ein..."
     Target_fr_fr "Saisissez votre nom..."
     Target_it_it "Inserisci il tuo cognome..."
     Target_es_es "Introduce tu apellido..."
     Target_pt_pt "Introduza o seu apelido..."
     Target_pt_br "Digite seu sobrenome..."
     Comment "Character creation last name field placeholder"
    }
  CustomStringTableItem "{BB9797D7BF69E9F2}" {
   Id "EL-CharacterCreation_Age"
   Target_en_us "Age:"
   Target_es_es "Edad:"
   Target_de_de "Alter:"
   Target_fr_fr "Âge :"
   Target_it_it "Età :"
   Target_pt_pt "Idade:"
   Target_pt_br "Idade:"
  }
CustomStringTableItem "{54AF4AE58A1DBCF7}" {
    Id "EL-CharacterCreation_Create"
    Target_en_us "Create Character"
    Target_es_es "Crear Personaje"
    Target_de_de "Charakter erstellen"
    Target_fr_fr "Créer le personnage"
    Target_it_it "Crea personaggio"
    Target_pt_pt "Criar personagem"
    Target_pt_br "Criar personagem"
   }
  CustomStringTableItem "{52C0B4BC0FA1B26D}" {
   Id "EL-CharacterCreation_FirstName"
   Target_en_us "First Name:"
   Target_es_es "Nombre:"
   Target_de_de "Vorname:"
   Target_fr_fr "Prénom :"
   Target_it_it "Nome :"
   Target_pt_pt "Nome próprio:"
   Target_pt_br "Primeiro nome:"
  }
  CustomStringTableItem "{32AE1891B7DEDCF6}" {
   Id "EL-CharacterCreation_LastName"
   Target_en_us "Last Name:"
   Target_es_es "Apellido:"
   Target_de_de "Nachname:"
   Target_fr_fr "Nom de famille :"
   Target_it_it "Cognome :"
   Target_pt_pt "Apelido:"
   Target_pt_br "Sobrenome:"
  }
  CustomStringTableItem "{89423F406555DECA}" {
   Id "EL-CharacterCreation_Title"
   Target_en_us "Create Your Character"
   Target_es_es "Crea tu Personaje"
   Target_de_de "Erstelle deinen Charakter"
   Target_fr_fr "Créez votre personnage"
   Target_it_it "Crea il tuo personaggio"
   Target_pt_pt "Cria o teu personagem"
   Target_pt_br "Crie seu personagem"
  }
  CustomStringTableItem "{528E67C0E6D6C58D}" {
   Id "EL-Confiscate_DisplayName"
   Target_en_us "Confiscate Weapons"
   Target_es_es "Confiscar Armas"
   Target_de_de "Waffen beschlagnahmen"
   Target_fr_fr "Confisquer les Armes"
   Target_it_it "Confisca Armi"
   Target_pt_pt "Confiscar Armas"
   Target_pt_br "Confiscar Armas"
  }
    CustomStringTableItem "{4D0E7310AF59E5DA}" {
     Id "EL-Confiscate_NoTarget"
     Target_en_us "No wanted player nearby"
     Comment "Confiscate action refused because no wanted player is within range"
    }
    CustomStringTableItem "{92FBA59A53BA6764}" {
     Id "EL-Confiscate_OnDutyOnly"
     Target_en_us "Only on-duty {police} can confiscate"
     Comment "Confiscate action refused because the player is not an on-duty officer"
    }
    CustomStringTableItem "{C27D5F9B1E804A64}" {
     Id "EL-Contraband_Deliver"
     Target_en_us "Deliver contraband"
     Comment "Convoy delivery action name and notification title"
    }
    CustomStringTableItem "{F5A082CE4B137D97}" {
     Id "EL-Contraband_Failed"
     Target_en_us "The convoy failed"
     Comment "Notification when a convoy run fails"
    }
    CustomStringTableItem "{D38E60AC2F915B75}" {
     Id "EL-Contraband_NotMember"
     Target_en_us "Only organization members can run this convoy"
     Comment "Convoy start reason when the player is not a member of the hideout organization"
    }
    CustomStringTableItem "{E49F71BD3A026C86}" {
     Id "EL-Contraband_Paid"
     Target_en_us "Cash paid for the delivery"
     Comment "Notification when a convoy delivery pays cash"
    }
    CustomStringTableItem "{B19C4E8A0D6F2753}" {
     Id "EL-Contraband_Start"
     Target_en_us "Start convoy"
     Comment "Convoy start action name"
    }
   CustomStringTableItem "{D5A0873EE40176C2}" {
    Id "EL-Death_Respawn"
    Target_en_us "Respawn"
    Target_de_de "Neu starten"
    Target_fr_fr "Réapparaître"
    Target_it_it "Ricomparizione"
    Target_es_es "Reaparecer"
    Target_pt_pt "Reaparecer"
     Target_pt_br "Reaparecer"
     Comment "Death screen respawn button"
   }
   CustomStringTableItem "{4CADF51C9441EA8A}" {
    Id "EL-Death_Title"
    Target_en_us "You are dead"
    Target_de_de "Du bist gestorben"
    Target_fr_fr "Vous êtes mort"
    Target_it_it "Sei morto"
    Target_es_es "Estás muerto"
    Target_pt_pt "Você morreu"
    Target_pt_br "Você morreu"
    Comment "Death screen title"
   }
   CustomStringTableItem "{6A52D1D87E9A31C4}" {
    Id "EL-DebugMenu_Close"
    Target_en_us "Close"
    Target_de_de "Schließen"
    Target_fr_fr "Fermer"
    Target_it_it "Chiudi"
    Target_es_es "Cerrar"
    Target_pt_pt "Fechar"
    Target_pt_br "Fechar"
    Comment "Debug menu close hint"
   }
   CustomStringTableItem "{BBED3D35CECA0BEE}" {
    Id "EL-DebugMenu_FactionCivilian"
    Target_en_us "Civilian Faction"
    Target_de_de "Zivilisten-Fraktion"
    Target_fr_fr "Fraction civile"
    Target_it_it "Fazione civile"
    Target_es_es "Facción civil"
    Target_pt_pt "Facção civil"
    Target_pt_br "Facção civil"
    Comment "Debug menu: set civilian faction"
   }
   CustomStringTableItem "{C72E71FF41583BB3}" {
    Id "EL-DebugMenu_FactionPolice"
    Target_en_us "Police Faction"
    Target_de_de "Polizei-Fraktion"
    Target_fr_fr "Fraction police"
    Target_it_it "Fazione polizia"
    Target_es_es "Facción policial"
    Target_pt_pt "Facção policial"
    Target_pt_br "Facção policial"
    Comment "Debug menu: set police faction"
   }
   CustomStringTableItem "{B314C31C663DB665}" {
    Id "EL-DebugMenu_GiveCash"
    Target_en_us "Give Cash +1000"
    Target_de_de "Geld geben +1000"
    Target_fr_fr "Donner argent +1000"
    Target_it_it "Dai denaro +1000"
    Target_es_es "Dar dinero +1000"
    Target_pt_pt "Dar dinheiro +1000"
    Target_pt_br "Dar dinheiro +1000"
    Comment "Debug menu: add cash button"
   }
   CustomStringTableItem "{84BD5C1286CDFFBF}" {
    Id "EL-DebugMenu_GrantSp"
    Target_en_us "Grant SP +5"
    Target_de_de "SP geben +5"
    Target_fr_fr "Donner SP +5"
    Target_it_it "Dai SP +5"
    Target_es_es "Dar SP +5"
    Target_pt_pt "Dar SP +5"
    Target_pt_br "Dar SP +5"
    Comment "Debug menu: add skill points"
   }
   CustomStringTableItem "{33D04535861495AB}" {
    Id "EL-DebugMenu_GrantXp"
    Target_en_us "Grant XP +500"
    Target_de_de "XP geben +500"
    Target_fr_fr "Donner XP +500"
    Target_it_it "Dai XP +500"
    Target_es_es "Dar XP +500"
    Target_pt_pt "Dar XP +500"
    Target_pt_br "Dar XP +500"
    Comment "Debug menu: add experience"
   }
   CustomStringTableItem "{5F3EB174B8DF0FBB}" {
    Id "EL-DebugMenu_JobFarmer"
    Target_en_us "Job: Farmer"
    Target_de_de "Job: Bauer"
    Target_fr_fr "Métier : agriculteur"
    Target_it_it "Lavoro: contadino"
    Target_es_es "Trabajo: granjero"
    Target_pt_pt "Trabalho: agricultor"
    Target_pt_br "Trabalho: agricultor"
    Comment "Debug menu: set farmer job"
   }
   CustomStringTableItem "{5D41B256D421B6E8}" {
    Id "EL-DebugMenu_JobUnemployed"
    Target_en_us "Job: Unemployed"
    Target_de_de "Job: Arbeitslos"
    Target_fr_fr "Métier : sans emploi"
    Target_it_it "Lavoro: disoccupato"
    Target_es_es "Trabajo: desempleado"
    Target_pt_pt "Trabalho: desempregado"
    Target_pt_br "Trabalho: desempregado"
    Comment "Debug menu: set unemployed job"
   }
   CustomStringTableItem "{63A127949F852CC4}" {
    Id "EL-DebugMenu_SurvivalEmpty"
    Target_en_us "Survival Empty"
    Target_de_de "Überleben leer"
    Target_fr_fr "Survie vide"
    Target_it_it "Sopravvivenza vuota"
    Target_es_es "Supervivencia vacía"
    Target_pt_pt "Sobrevivência vazia"
    Target_pt_br "Sobrevivência vazia"
    Comment "Debug menu: zero hunger/thirst/health"
   }
   CustomStringTableItem "{836DE3CEE3A9A308}" {
    Id "EL-DebugMenu_SurvivalFull"
    Target_en_us "Survival Full"
    Target_de_de "Überleben voll"
    Target_fr_fr "Survie complète"
    Target_it_it "Sopravvivenza piena"
    Target_es_es "Supervivencia llena"
    Target_pt_pt "Sobrevivência cheia"
    Target_pt_br "Sobrevivência cheia"
    Comment "Debug menu: max hunger/thirst/health"
   }
   CustomStringTableItem "{58B6B04D77AF1CCC}" {
    Id "EL-DebugMenu_TakeCash"
    Target_en_us "Take Cash -1000"
    Target_de_de "Geld nehmen -1000"
    Target_fr_fr "Retirer argent -1000"
    Target_it_it "Togli denaro -1000"
    Target_es_es "Quitar dinero -1000"
    Target_pt_pt "Tirar dinheiro -1000"
    Target_pt_br "Tirar dinheiro -1000"
    Comment "Debug menu: remove cash button"
   }
    CustomStringTableItem "{40526487807838C7}" {
    Id "EL-DebugMenu_Title"
    Target_en_us "Debug Menu"
    Target_de_de "Debug-Menü"
    Target_fr_fr "Menu de débogage"
    Target_it_it "Menu di debug"
    Target_es_es "Menú de depuración"
    Target_pt_pt "Menu de depuração"
    Target_pt_br "Menu de depuração"
    Comment "Debug menu title"
   }
   CustomStringTableItem "{2DBD1E711A1FE9C9}" {
    Id "EL-DebugMenu_Wanted0"
    Target_en_us "Wanted 0"
    Target_de_de "Gesucht 0"
    Target_fr_fr "Recherché 0"
    Target_it_it "Ricercato 0"
    Target_es_es "Buscado 0"
    Target_pt_pt "Procurado 0"
    Target_pt_br "Procurado 0"
    Comment "Debug menu: clear wanted level"
   }
   CustomStringTableItem "{EB6A833DCB4086E3}" {
    Id "EL-DebugMenu_Wanted5"
    Target_en_us "Wanted 5"
    Target_de_de "Gesucht 5"
    Target_fr_fr "Recherché 5"
    Target_it_it "Ricercato 5"
    Target_es_es "Buscado 5"
    Target_pt_pt "Procurado 5"
    Target_pt_br "Procurado 5"
    Comment "Debug menu: max wanted level"
   }
   CustomStringTableItem "{B8AE8B1EC379425C}" {
    Id "EL-DeleteAction_DisplayName"
    Target_en_us "Destroy"
    Target_de_de "Zerstören"
    Target_fr_fr "Détruire"
    Target_it_it "Distruggi"
    Target_es_es "Destruir"
    Target_pt_pt "Destruir"
    Target_pt_br "Destruir"
    Modified 1788019742
    Author "LifeFramework"
    LastChanged "LifeFramework"
   }
    CustomStringTableItem "{D302B585166A8EAE}" {
     Id "EL-Duty_GoOff"
     Target_en_us "Go off duty"
     Comment "Police duty toggle action name while the officer is on duty"
    }
    CustomStringTableItem "{BDFF965656B478E0}" {
     Id "EL-Duty_GoOn"
     Target_en_us "Go on duty"
     Comment "Police duty toggle action name while the officer is off duty"
    }
    CustomStringTableItem "{F4D6F78E634D1214}" {
     Id "EL-Duty_Paid"
     Target_en_us "Duty pay: {currency:sym}%1 cash"
     Comment "Toast after the station desk hands an officer going off duty their shift pay in cash; %1 is the amount"
    }
    CustomStringTableItem "{C63996AB2CF2559D}" {
     Id "EL-Duty_PoliceOnly"
     Target_en_us "Only police can change duty"
     Comment "Duty toggle refused because the player is not in the police faction"
    }
  CustomStringTableItem "{E9C8FC3D37EA4FF9}" {
   Id "EL-Duty_Status"
   Target_en_us "Duty Status"
   Target_es_es "Estado de Servicio"
   Target_de_de "Dienststatus"
   Target_fr_fr "Statut de Service"
   Target_it_it "Stato di Servizio"
   Target_pt_pt "Estado de Serviço"
   Target_pt_br "Estado de Serviço"
  }
  CustomStringTableItem "{E707FDC68C849C38}" {
   Id "EL-Duty_Status_Change"
   Target_en_us "You are now %1"
   Target_es_es "Ahora eres %1"
   Target_de_de "Du bist jetzt %1"
   Target_fr_fr "Vous êtes maintenant %1"
   Target_it_it "Ora sei %1"
   Target_pt_pt "Agora és %1"
   Target_pt_br "Agora você é %1"
  }
    CustomStringTableItem "{4F8C2A91D6B74E12}" {
     Id "EL-Escort_Arrest"
     Target_en_us "Detain for escort"
     Comment "Officer detains a wanted suspect for vehicle escort. This does not start the jail sentence."
    }
    CustomStringTableItem "{4F8C2A91D6B74E11}" {
     Id "EL-Escort_Grab"
     Target_en_us "Escort"
     Comment "Officer walks a wanted suspect toward a vehicle. The sentence has not started."
    }
    CustomStringTableItem "{4F8C2A91D6B74E15}" {
     Id "EL-Escort_NoPrisoner"
     Target_en_us "No prisoner to escort"
     Comment "Vehicle escort action refused because the officer has no prisoner on foot or in a seat"
    }
    CustomStringTableItem "{4F8C2A91D6B74E13}" {
     Id "EL-Escort_PutIn"
     Target_en_us "Put in vehicle"
     Comment "Officer seats the escorted prisoner in the nearest free passenger seat"
    }
    CustomStringTableItem "{4F8C2A91D6B74E14}" {
     Id "EL-Escort_TakeOut"
     Target_en_us "Take out of vehicle"
     Comment "Officer removes the prisoner from the vehicle. The sentence starts only at the cell."
    }
   CustomStringTableItem "{A8730ED008B83986}" {
    Id "EL-Faction_Civilian"
    Target_en_us "Civilian"
    Target_es_es "Civil"
    Target_de_de "Zivilist"
    Target_fr_fr "Civil"
    Target_it_it "Civile"
    Target_pt_pt "Civil"
    Target_pt_br "Civil"
   }
   CustomStringTableItem "{8D4E3C7BBC67A1B5}" {
    Id "EL-Faction_Police"
    Target_en_us "Police"
    Target_es_es "Policía"
    Target_de_de "Polizei"
    Target_fr_fr "Police"
    Target_it_it "Polizia"
    Target_pt_pt "Polícia"
    Target_pt_br "Polícia"
   }
   CustomStringTableItem "{71BBA650CD61B241}" {
    Id "EL-Faction_Title"
    Target_en_us "Select Faction"
    Target_es_es "Seleccionar facción"
    Target_de_de "Fraktion wählen"
    Target_fr_fr "Choisir une faction"
    Target_it_it "Seleziona fazione"
    Target_pt_pt "Selecionar facção"
    Target_pt_br "Selecionar facção"
   }
  CustomStringTableItem "{3786BE1BEAA9D0BE}" {
   Id "EL-Fine_Failed"
   Target_en_us "Player doesn't have enough money!"
   Target_es_es "El jugador no tiene suficiente dinero!"
   Target_de_de "Der Spieler hat nicht genug Geld!"
   Target_fr_fr "Le joueur n'a pas assez d'argent!"
   Target_it_it "Il giocatore non ha abbastanza denaro!"
   Target_pt_pt "O jogador não tem dinheiro suficiente!"
   Target_pt_br "O jogador não tem dinheiro suficiente!"
  }
    CustomStringTableItem "{41C6EEAFCB6BAA99}" {
     Id "EL-Fine_Title"
     Target_en_us "Fine Failed"
     Target_de_de "Bußgeld fehlgeschlagen"
     Target_fr_fr "Amende échouée"
     Target_it_it "Multa non riuscita"
     Target_es_es "Multa fallida"
     Target_pt_pt "Multa falhada"
     Target_pt_br "Multa falhou"
     Comment "Notification title when a fine cannot be issued"
    }
    CustomStringTableItem "{DEBD888942A8B233}" {
     Id "EL-Fine_TooSmall"
     Target_en_us "A fine must be at least 1000 to lower the wanted level"
     Comment "Police fine refused because the amount is below one wanted level (1000 cash per level)"
    }
  CustomStringTableItem "{5B715E486235B275}" {
   Id "EL-Fined"
   Target_en_us "Fined"
   Target_es_es "Multado"
   Target_de_de "Gebüßt"
   Target_fr_fr "Amende"
   Target_it_it "Multato"
   Target_pt_pt "Multado"
   Target_pt_br "Multado"
  }
    CustomStringTableItem "{685890AF28A71E11}" {
     Id "EL-Fuel_Till_Rob"
     Target_en_us "Rob the till"
     Comment "Fuel station till robbery action name"
    }
    CustomStringTableItem "{6CB2BEC3153A8FC2}" {
     Id "EL-Fuel_Till_Rob_Desc"
     Target_en_us "Take the cash from the till"
     Comment "Fuel station till robbery action description"
    }
    CustomStringTableItem "{8C3E1A7F5D2B9046}" {
     Id "EL-Hideout_NoUpgrade"
     Target_en_us "The hideout needs the processing upgrade"
     Comment "Process action reason when the hideout organization lacks the processing slot"
    }
    CustomStringTableItem "{6F1A8C2D4B9E7031}" {
     Id "EL-Hideout_NotMember"
     Target_en_us "Only organization members can use this door"
     Comment "Door action reason when the player is not in the hideout organization"
    }
    CustomStringTableItem "{A4D70E2C9B15F683}" {
     Id "EL-Hideout_Process"
     Target_en_us "Process contraband"
     Comment "Hideout machine process action name"
    }
    CustomStringTableItem "{0C37169F45E8A2BD}" {
     Id "EL-Hospital_BreachCabinet"
     Target_en_us "Breach the cabinet"
     Comment "Hospital cabinet breach action name"
    }
    CustomStringTableItem "{B7E2C14A90F35D68}" {
     Id "EL-Hospital_RobTill"
     Target_en_us "Rob the till"
     Comment "Hospital till robbery action name"
    }
    CustomStringTableItem "{EA15F47D23C6809B}" {
     Id "EL-Hospital_TillCivilianOnly"
     Target_en_us "Only civilians can rob the till"
     Comment "Hospital till action refused for a non-civilian"
    }
    CustomStringTableItem "{D904E36C12B57F8A}" {
     Id "EL-Hospital_TillCooldown"
     Target_en_us "The till was just robbed"
     Comment "Hospital till action refused because the cooldown is still running"
    }
    CustomStringTableItem "{FB26058E34D791AC}" {
     Id "EL-Hospital_TillNeedPolice"
     Target_en_us "Not enough {police} are on duty"
     Comment "Hospital till action refused below the minimum police count"
    }
    CustomStringTableItem "{C8F3D25B01A46E79}" {
     Id "EL-Hospital_TillNotConfigured"
     Target_en_us "This till is not set up"
     Comment "Hospital till action refused because the till is not configured"
    }
    CustomStringTableItem "{3E2F5AFF213F921E}" {
     Id "EL-HouseLock_Reason"
     Target_en_us "House is locked"
     Target_de_de "Das Haus ist abgeschlossen"
     Target_fr_fr "La maison est verrouillée"
     Target_it_it "La casa è bloccata"
     Target_es_es "La casa está bloqueada"
     Target_pt_pt "A casa está trancada"
     Target_pt_br "A casa está trancada"
     Comment "Door action cannot-perform reason on a locked house"
    }
  CustomStringTableItem "{04BE9409328F0209}" {
   Id "EL-ID_Information"
   Target_en_us "ID Information"
   Target_es_es "Información de ID"
   Target_de_de "ID-Information"
   Target_fr_fr "Information d'ID"
   Target_it_it "Informazione ID"
   Target_pt_pt "Informação de ID"
   Target_pt_br "Informação de ID"
  }
  CustomStringTableItem "{4CC21547F7D0E8C0}" {
   Id "EL-Inventory_Full"
   Target_en_us "Inventory full!"
   Target_es_es "Inventario lleno!"
   Target_de_de "Inventar voll!"
   Target_fr_fr "Inventaire plein!"
   Target_it_it "Inventario pieno!"
   Target_pt_pt "Inventário cheio!"
   Target_pt_br "Inventário cheio!"
  }
    CustomStringTableItem "{EC4658E696C73E74}" {
     Id "EL-ItemDesc_HouseKey"
     Target_en_us "Key for the house you own"
     Target_de_de "Schlüssel für dein eigenes Haus"
     Target_fr_fr "Clé de la maison que vous possédez"
     Target_it_it "Chiave della casa di tua proprietà"
     Target_es_es "Llave de la casa de tu propiedad"
     Target_pt_pt "Chave da casa da sua propriedade"
     Target_pt_br "Chave da casa da sua propriedade"
     Comment "House key item description"
    }
    CustomStringTableItem "{B908E0A780049A70}" {
     Id "EL-ItemName_HouseKey"
     Target_en_us "House Key"
     Target_de_de "Hausschlüssel"
     Target_fr_fr "Clé de maison"
     Target_it_it "Chiave di casa"
     Target_es_es "Llave de casa"
     Target_pt_pt "Chave de casa"
     Target_pt_br "Chave de casa"
     Comment "House key item name"
    }
CustomStringTableItem "{AA76A797BE84093A}" {
    Id "EL-Item_Bought"
    Target_en_us "Bought %1 x%2!"
    Target_es_es "Compró %1 x%2!"
    Target_de_de "%1 x%2 gekauft!"
    Target_fr_fr "Acheté %1 x%2!"
    Target_it_it "Comprato %1 x%2!"
    Target_pt_pt "Comprou %1 x%2!"
    Target_pt_br "Comprou %1 x%2!"
   }
   CustomStringTableItem "{281B36AB823C412B}" {
    Id "EL-Item_Pickaxe_Description"
    Target_en_us "Tool for mining ore"
    Target_de_de "Werkzeug zum Erzabbau"
    Target_fr_fr "Outil pour miner le minerai"
    Target_it_it "Attrezzo per estrarre minerale"
    Target_es_es "Herramienta para extraer mineral"
    Target_pt_pt "Ferramenta para minerar minério"
    Target_pt_br "Ferramenta para minerar minério"
    Comment "Pickaxe tool description"
    Modified 1788019742
    Author "LifeFramework"
    LastChanged "LifeFramework"
   }
   CustomStringTableItem "{3A70AF4FE08F42F5}" {
    Id "EL-Item_Pickaxe_Name"
    Target_en_us "Pickaxe"
    Target_de_de "Spitzhacke"
    Target_fr_fr "Pioche"
    Target_it_it "Piccone"
    Target_es_es "Pico"
    Target_pt_pt "Picareta"
    Target_pt_br "Picareta"
    Comment "Pickaxe tool name"
    Modified 1788019742
    Author "LifeFramework"
    LastChanged "LifeFramework"
   }
  CustomStringTableItem "{FDB3A4352A9457A4}" {
   Id "EL-Item_Sold"
   Target_en_us "Sold for {currency:sym}%1!"
   Target_es_es "Item vendido por {currency:sym}%1!"
   Target_de_de "Item für {currency:sym}%1 verkauft!"
   Target_fr_fr "Objet vendu pour {currency:sym}%1!"
   Target_it_it "Oggetto venduto per {currency:sym}%1!"
   Target_pt_pt "Item vendido por {currency:sym}%1!"
   Target_pt_br "Item vendido por {currency:sym}%1!"
  }
    CustomStringTableItem "{F1030FBC0E45B177}" {
     Id "EL-Jail_Arrest"
     Target_en_us "Send to jail"
     Comment "Officer action on a wanted suspect"
    }
    CustomStringTableItem "{80654DEEF27895C6}" {
     Id "EL-Jail_EarlyRelease"
     Target_en_us "The sentence is not over"
     Comment "Early release refused while time remains"
    }
    CustomStringTableItem "{70C09B02D265C0E8}" {
     Id "EL-Jail_NoCell"
     Target_en_us "No jail cell is placed"
     Comment "Jail action refused because the world has no cell marker"
    }
    CustomStringTableItem "{8AEA48F56B9C5F59}" {
     Id "EL-Jail_NotOfficer"
     Target_en_us "Only on-duty {police} can send someone to jail"
     Comment "Jail action refused because the user is not an on-duty officer"
    }
    CustomStringTableItem "{C1D59E0ED7B03055}" {
     Id "EL-Jail_NotWanted"
     Target_en_us "This person is not wanted"
     Comment "Jail action refused because the suspect is not wanted or is out of range"
    }
    CustomStringTableItem "{D588BDC3746521B7}" {
     Id "EL-Jail_Released"
     Target_en_us "Sentence served"
     Comment "Notification when a jail sentence ends"
    }
    CustomStringTableItem "{A11C0B01E4A10008}" {
     Id "EL-Job_Abandon"
     Target_en_us "Abandon the run"
     Comment "Workplace action that drops the active run with no pay"
    }
    CustomStringTableItem "{A11C0B01E4A10010}" {
     Id "EL-Job_Abandoned"
     Target_en_us "Run abandoned. No pay."
     Comment "Notice when a delivery run is abandoned"
    }
    CustomStringTableItem "{A11C0B01E4A10007}" {
     Id "EL-Job_Dropoff"
     Target_en_us "Complete the run"
     Comment "Drop marker action that pays a delivery run"
    }
  CustomStringTableItem "{C60F134F56CDE9BE}" {
   Id "EL-Job_Earned"
   Target_en_us "Earned {currency:sym}%1 for work!"
   Target_es_es "Ganó {currency:sym}%1 por trabajo!"
   Target_de_de "{currency:sym}%1 für Arbeit verdient!"
   Target_fr_fr "Gagné {currency:sym}%1 pour le travail!"
   Target_it_it "Guadagnato {currency:sym}%1 per lavoro!"
   Target_pt_pt "Ganhou {currency:sym}%1 pelo trabalho!"
   Target_pt_br "Ganhou {currency:sym}%1 pelo trabalho!"
  }
    CustomStringTableItem "{A11C0B01E4A10009}" {
     Id "EL-Job_LevelRequired"
     Target_en_us "Your work level is too low"
     Comment "Signup refused because the civilian career level is too low"
    }
    CustomStringTableItem "{A11C0B01E4A1000C}" {
     Id "EL-Job_Name_Courier"
     Target_en_us "Courier"
     Comment "Job name for parcel delivery"
    }
    CustomStringTableItem "{A11C0B01E4A1000D}" {
     Id "EL-Job_Name_Fuel"
     Target_en_us "Fuel delivery"
     Comment "Job name for fuel delivery"
    }
    CustomStringTableItem "{A11C0B01E4A1000E}" {
     Id "EL-Job_Name_Taxi"
     Target_en_us "Taxi driver"
     Comment "Job name for taxi work"
    }
    CustomStringTableItem "{A11C0B01E4A1000B}" {
     Id "EL-Job_NoActiveRun"
     Target_en_us "You have no run to finish"
     Comment "Drop or abandon refused because no run is active"
    }
    CustomStringTableItem "{A11C0B01E4A1000A}" {
     Id "EL-Job_NotSignedUp"
     Target_en_us "Sign up at the workplace first"
     Comment "Run refused because the player has not taken this job"
    }
    CustomStringTableItem "{A11C0B01E4A1000F}" {
     Id "EL-Job_Paid"
     Target_en_us "Run paid: {currency:sym}%1"
     Comment "Cash notice after a completed delivery run"
    }
    CustomStringTableItem "{A11C0B01E4A10005}" {
     Id "EL-Job_Pickup_Fuel"
     Target_en_us "Take fuel can"
     Comment "Fuel depot action that starts a fuel delivery"
    }
    CustomStringTableItem "{A11C0B01E4A10004}" {
     Id "EL-Job_Pickup_Parcel"
     Target_en_us "Pick up parcel"
     Comment "Depot action that starts a parcel run"
    }
  CustomStringTableItem "{8F253E3EE2D8A786}" {
   Id "EL-Job_Reward"
   Target_en_us "Job Reward"
   Target_es_es "Recompensa de Trabajo"
   Target_de_de "Arbeitsbelohnung"
   Target_fr_fr "Récompense de Travail"
   Target_it_it "Ricompensa Lavoro"
   Target_pt_pt "Recompensa de Trabalho"
   Target_pt_br "Recompensa de Trabalho"
  }
    CustomStringTableItem "{A11C0B01E4A10001}" {
     Id "EL-Job_SignUp_Courier"
     Target_en_us "Take courier work"
     Comment "Workplace action to sign up for parcel delivery"
    }
    CustomStringTableItem "{A11C0B01E4A10002}" {
     Id "EL-Job_SignUp_Fuel"
     Target_en_us "Take fuel delivery"
     Comment "Workplace action to sign up for fuel delivery"
    }
    CustomStringTableItem "{A11C0B01E4A10003}" {
     Id "EL-Job_SignUp_Taxi"
     Target_en_us "Take taxi work"
     Comment "Workplace action to sign up for taxi work"
    }
    CustomStringTableItem "{A11C0B01E4A10006}" {
     Id "EL-Job_Start_Taxi"
     Target_en_us "Start fare"
     Comment "Taxi rank action that starts a marker fare"
    }
  CustomStringTableItem "{5A60AE2B8D435CE9}" {
   Id "EL-Keybinds_Inventory_MoveQuantity"
   Target_en_us "Move stack"
   Target_de_de "Stapel verschieben"
   Target_fr_fr "Déplacer la pile"
   Target_it_it "Sposta pila"
   Target_es_es "Mover pila"
   Target_pt_pt "Mover pilha"
   Target_pt_br "Mover pilha"
   Modified 1516407434
   Author "Arkensor"
   LastChanged "Arkensor"
  }
  CustomStringTableItem "{5A5EA70E9C33566A}" {
   Id "EL-Keybinds_Inventory_PickUpQuantity"
   Target_en_us "Pick up stack"
   Target_de_de "Stapel aufsammeln"
   Target_fr_fr "Ramasser la pile"
   Target_it_it "Raccogli pila"
   Target_es_es "Recoger pila"
   Target_pt_pt "Pegar pilha"
   Target_pt_br "Pegar pilha"
   Modified 1516407428
   Author "Arkensor"
   LastChanged "Arkensor"
  }
  CustomStringTableItem "{5A628A92C80C8905}" {
   Id "EL-Keybinds_Inventory_QuickSplitQuantity"
   Target_en_us "Half stack"
   Target_de_de "Stapel halbieren"
   Target_fr_fr "Demi-pile"
   Target_it_it "Mezza pila"
   Target_es_es "Mitad de pila"
   Target_pt_pt "Meia pilha"
   Target_pt_br "Meia pilha"
   Modified 1516408899
   Author "Arkensor"
   LastChanged "Arkensor"
  }
  CustomStringTableItem "{5A628A92CC3D121C}" {
   Id "EL-Keybinds_Inventory_SplitQuantity"
   Target_en_us "Split stack"
   Target_de_de "Stapel aufteilen"
   Target_fr_fr "Diviser la pile"
   Target_it_it "Dividi pila"
   Target_es_es "Dividir pila"
   Target_pt_pt "Dividir pilha"
   Target_pt_br "Dividir pilha"
   Modified 1516408876
   Author "Arkensor"
   LastChanged "Arkensor"
  }
   CustomStringTableItem "{47776A7324DF510E}" {
    Id "EL-Keybinds_OpenDebugMenu"
    Target_en_us "Open debug menu"
    Target_de_de "Debug-Menü öffnen"
    Target_fr_fr "Ouvrir le menu de débogage"
    Target_it_it "Apri il menu di debug"
    Target_es_es "Abrir menú de depuración"
    Target_pt_pt "Abrir menu de depuração"
    Target_pt_br "Abrir menu de depuração"
    Comment "Keybind: open the debug menu"
   }
  CustomStringTableItem "{5A5EA70D1ADCCA9E}" {
   Id "EL-Keybinds_Title"
   Target_en_us "Life Framework"
   Modified 1516153387
   Author "Arkensor"
   LastChanged "Arkensor"
  }
  CustomStringTableItem "{5AA164EA498BDCE0}" {
   Id "EL-Not_Enough_Money"
   Target_en_us "Not enough money!"
   Target_es_es "Dinero insuficiente!"
   Target_de_de "Nicht genug Geld!"
   Target_fr_fr "Pas assez d'argent!"
   Target_it_it "Denaro insufficiente!"
   Target_pt_pt "Dinheiro insuficiente!"
   Target_pt_br "Dinheiro insuficiente!"
  }
    CustomStringTableItem "{BD58B67D6B3A0F31}" {
     Id "EL-Org_Amount"
     Target_en_us "Amount"
     Comment "Organization cash amount field"
    }
    CustomStringTableItem "{7B882168E8E2EA56}" {
     Id "EL-Org_BuyProcessing"
     Target_en_us "Buy processing slot"
     Comment "Organization processing-slot purchase button"
    }
    CustomStringTableItem "{32C8D2BD6064F36A}" {
     Id "EL-Org_Close"
     Target_en_us "Close"
     Comment "Organization menu close button"
    }
    CustomStringTableItem "{10658CE04572E9FD}" {
     Id "EL-Org_Create"
     Target_en_us "Found organization"
     Comment "Organization create button"
    }
    CustomStringTableItem "{BCA58FF32AF18A97}" {
     Id "EL-Org_Deposit"
     Target_en_us "Deposit cash"
     Comment "Organization fund deposit button"
    }
    CustomStringTableItem "{7415A5742F78A08C}" {
     Id "EL-Org_Empty"
     Target_en_us "No organization yet"
     Comment "Organization menu empty state before create"
    }
    CustomStringTableItem "{8286DFFBEFD2CF59}" {
     Id "EL-Org_Fund"
     Target_en_us "Fund"
     Comment "Organization shared fund row"
    }
    CustomStringTableItem "{BCD86E4C6B473ACD}" {
     Id "EL-Org_Name"
     Target_en_us "Name"
     Comment "Organization name row"
    }
    CustomStringTableItem "{1A7D101AF53FF056}" {
     Id "EL-Org_NamePlaceholder"
     Target_en_us "Organization name"
     Comment "Organization name field placeholder"
    }
    CustomStringTableItem "{232AC988385CE40A}" {
     Id "EL-Org_Open"
     Target_en_us "Organization desk"
     Comment "Organization desk action name"
    }
    CustomStringTableItem "{5EC419CBC808514E}" {
     Id "EL-Org_Title"
     Target_en_us "Organization"
     Comment "Organization menu title"
    }
    CustomStringTableItem "{9922728E793CD0D8}" {
     Id "EL-Org_TooFar"
     Target_en_us "Stand at the organization desk to do that"
     Comment "Organization fund deposit or withdraw refused because the player walked away from the desk"
    }
    CustomStringTableItem "{72E88124F3AF33E9}" {
     Id "EL-Org_UpgradeNone"
     Target_en_us "Processing slot: not owned"
     Comment "Organization processing-slot upgrade when not bought"
    }
    CustomStringTableItem "{40AA137751D95438}" {
     Id "EL-Org_UpgradeOwned"
     Target_en_us "Processing slot: owned"
     Comment "Organization processing-slot upgrade when bought"
    }
    CustomStringTableItem "{4550F13909B7A733}" {
     Id "EL-Org_Withdraw"
     Target_en_us "Withdraw cash"
     Comment "Organization fund withdraw button"
    }
  CustomStringTableItem "{8A914631969AA29B}" {
   Id "EL-Player_Arrested"
   Target_en_us "Player arrested!"
   Target_es_es "Jugador arrestado!"
   Target_de_de "Spieler verhaftet!"
   Target_fr_fr "Joueur arrêté!"
   Target_it_it "Giocatore arrestato!"
   Target_pt_pt "Jogador preso!"
   Target_pt_br "Jogador preso!"
  }
    CustomStringTableItem "{6C34F8044BFC1E91}" {
     Id "EL-PoliceAlert_Title"
     Target_en_us "Police Alert"
     Target_de_de "Polizeialarm"
     Target_fr_fr "Alerte police"
     Target_it_it "Allarme polizia"
     Target_es_es "Alerta policial"
     Target_pt_pt "Alerta policial"
     Target_pt_br "Alerta policial"
     Comment "Notification title for crime alerts to police"
    }
  CustomStringTableItem "{D805FA4B449DCBDB}" {
   Id "EL-Police_Action"
   Target_en_us "Weapons confiscated!"
   Target_es_es "Armas confiscadas!"
   Target_de_de "Waffen beschlagnahmt!"
   Target_fr_fr "Armes confisquées!"
   Target_it_it "Armi confiscate!"
   Target_pt_pt "Armas confiscadas!"
   Target_pt_br "Armas confiscadas!"
  }
    CustomStringTableItem "{C4E8A17B2D9056F3}" {
     Id "EL-Police_Alert_Breach"
     Target_en_us "Crime Alert: Explosive charge placed"
     Comment "Police notification when a breach charge is armed"
    }
    CustomStringTableItem "{1D4827A056F9B3CE}" {
     Id "EL-Police_Alert_Hospital"
     Target_en_us "Crime Alert: Hospital till robbed"
     Comment "Police notification when the hospital till is robbed"
    }
  CustomStringTableItem "{29AE645903F2735C}" {
   Id "EL-Police_Alert_Robbery"
   Target_en_us "Crime Alert: Robbery in progress!"
   Target_es_es "Alerta de Crimen: Robo en progreso!"
   Target_de_de "Verbrechensalarm: Raub im Gange!"
   Target_fr_fr "Alerte Crime: Vol en cours!"
   Target_it_it "Allarme Crimine: Rapina in corso!"
   Target_pt_pt "Alerta de Crime: Roubo em andamento!"
   Target_pt_br "Alerta de Crime: Roubo em andamento!"
  }
  CustomStringTableItem "{94F7D789515A802D}" {
   Id "EL-Police_Alert_Vehicle"
   Target_en_us "Crime Alert: Vehicle theft!"
   Target_es_es "Alerta de Crimen: Robo de vehículo!"
   Target_de_de "Verbrechensalarm: Fahrzeugraub!"
   Target_fr_fr "Alerte Crime: Vol de véhicule!"
   Target_it_it "Allarme Crimine: Furto di veicolo!"
   Target_pt_pt "Alerta de Crime: Roubo de veículo!"
   Target_pt_br "Alerta de Crime: Roubo de veículo!"
  }
  CustomStringTableItem "{DA10C8D24DAFB028}" {
   Id "EL-Police_Alert_Weapon"
   Target_en_us "Crime Alert: Weapon theft!"
   Target_es_es "Alerta de Crimen: Robo de arma!"
   Target_de_de "Verbrechensalarm: Waffenraub!"
   Target_fr_fr "Alerte Crime: Vol d'arme!"
   Target_it_it "Allarme Crimine: Furto di arma!"
   Target_pt_pt "Alerta de Crime: Roubo de arma!"
   Target_pt_br "Alerta de Crime: Roubo de arma!"
  }
    CustomStringTableItem "{72732E59A99DA11E}" {
     Id "EL-Police_Title"
     Target_en_us "{Police} Action"
     Target_de_de "{Police}aktion"
     Target_fr_fr "Action de {police}"
     Target_it_it "Azione di {police}"
     Target_es_es "Acción policial"
     Target_pt_pt "Ação policial"
     Target_pt_br "Ação policial"
     Comment "Notification title for police actions"
    }
    CustomStringTableItem "{200D9FF1811EAA61}" {
     Id "EL-Police_TooFar"
     Target_en_us "The suspect is too far away"
     Comment "Police menu arrest or fine refused because the target is not within 5 m of the officer or is not connected"
    }
    CustomStringTableItem "{9D15B91990A1723B}" {
     Id "EL-Rank_AlreadyDrawn"
     Target_en_us "You already drew your kit this shift"
     Comment "Police armory refused because the officer drew a kit this duty shift. Going off duty resets it."
    }
    CustomStringTableItem "{C91A4E7700B10017}" {
     Id "EL-Rank_ArrestDenied"
     Target_en_us "Your rank cannot arrest"
     Comment "Arrest refused because the officer's rank cannot arrest"
    }
    CustomStringTableItem "{C91A4E7700B10013}" {
     Id "EL-Rank_Draw"
     Target_en_us "Draw kit"
     Comment "Station armory action that issues the officer's rank kit"
    }
    CustomStringTableItem "{161F069D20EEEF68}" {
     Id "EL-Rank_DrawCooldown"
     Target_en_us "The armory issues your next kit in %1 min"
     Comment "Toast: armory draw refused, the officer drew a kit less than 2 hours ago (real time). %1 = minutes left."
    }
    CustomStringTableItem "{C91A4E7700B10014}" {
     Id "EL-Rank_DrawDenied"
     Target_en_us "Only on-duty {police} can draw a kit"
     Comment "Armory draw refused"
    }
    CustomStringTableItem "{C91A4E7700B10018}" {
     Id "EL-Rank_FineDenied"
     Target_en_us "That fine is above your rank"
     Comment "Fine refused because it is above the officer's rank cap"
    }
    CustomStringTableItem "{5F2C8E1B7D4A9306}" {
     Id "EL-Rank_IssueNeedsRank"
     Target_en_us "The armory issues that from the rank of %1"
     Comment "Toast: the armory refused an item because the officer's rank is below the offer's. %1 = the rank title that may draw it."
    }
    CustomStringTableItem "{8A7BC982642B2C23}" {
     Id "EL-Rank_KitOutstanding"
     Target_en_us "Your last kit is still out. Hand it in here, or use the armory again to report it lost"
     Comment "Toast: armory draw refused, the officer has issued items not yet turned in. A second use within 15 s reports them lost."
    }
    CustomStringTableItem "{1DC38F5132E43BEC}" {
     Id "EL-Rank_KitPartReturned"
     Target_en_us "Kit handed in, %1 item(s) still out"
     Comment "Toast: the armory took back part of the issued kit. %1 = serialled items still missing."
    }
    CustomStringTableItem "{94F584B3EA53013B}" {
     Id "EL-Rank_KitReportedLost"
     Target_en_us "Kit reported lost. Its serials are logged"
     Comment "Toast: the officer confirmed the issued kit is lost; the book clears it and logs the serials. The 2 hour cooldown still applies."
    }
    CustomStringTableItem "{1DFCFD19D6D4381C}" {
     Id "EL-Rank_KitReturned"
     Target_en_us "Kit handed in"
     Comment "Toast: the armory took back every serialled item of the officer's issued kit"
    }
    CustomStringTableItem "{C91A4E7700B10011}" {
     Id "EL-Rank_Patrol"
     Target_en_us "Patrol Officer"
     Comment "Starting police rank title"
    }
    CustomStringTableItem "{C91A4E7700B10015}" {
     Id "EL-Rank_Promote"
     Target_en_us "Promote officer"
     Comment "Higher-rank action that promotes a nearby lower-rank officer"
    }
    CustomStringTableItem "{C91A4E7700B10016}" {
     Id "EL-Rank_PromoteDenied"
     Target_en_us "A higher rank has to authorize that"
     Comment "Promotion refused"
    }
    CustomStringTableItem "{BB32E46F8C493D2C}" {
     Id "EL-Rank_PromoteNeedsHigher"
     Target_en_us "Only an officer above the rank you are promoting into can promote"
     Comment "Toast to the promoter: the promotion was refused because their rank does not outrank the rank the target would reach."
    }
    CustomStringTableItem "{C91A4E7700B10012}" {
     Id "EL-Rank_Senior"
     Target_en_us "Senior Officer"
     Comment "Senior police rank title"
    }
    CustomStringTableItem "{C91A4E7700B10019}" {
     Id "EL-Rank_WantedListDenied"
     Target_en_us "Only on-duty {police} can view the wanted list"
     Comment "Wanted list refused for someone who is not on-duty police"
    }
    CustomStringTableItem "{C9C467388DE54730}" {
     Id "EL-Resource_IronTrade_Desc"
     Target_en_us "Sell smelted iron for cash"
     Comment "Storage description for the St Philippe iron-bar crate"
    }
    CustomStringTableItem "{154EF8AA34D04940}" {
     Id "EL-Resource_IronTrade_Name"
     Target_en_us "Iron bar trade"
     Comment "Storage name for the St Philippe iron-bar crate"
    }
    CustomStringTableItem "{EF39A7AF0980446C}" {
     Id "EL-Resource_JuiceTrade_Desc"
     Target_en_us "Sell juice for cash"
     Comment "Storage description for the St Philippe juice crate"
    }
    CustomStringTableItem "{9C72272DB847457E}" {
     Id "EL-Resource_JuiceTrade_Name"
     Target_en_us "Apple juice trade"
     Comment "Storage name for the St Philippe juice crate"
    }
    CustomStringTableItem "{11202505DD474762}" {
     Id "EL-Resource_TradeIron"
     Target_en_us "Trade iron bars"
     Comment "Action name on the St Philippe iron-bar trader"
    }
    CustomStringTableItem "{8D04BB7153344934}" {
     Id "EL-Resource_TradeIron_Desc"
     Target_en_us "Sell iron bars for cash"
     Comment "Action description on the St Philippe iron-bar trader"
    }
    CustomStringTableItem "{045DF984ED294440}" {
     Id "EL-Resource_TradeJuice"
     Target_en_us "Trade apple juice"
     Comment "Action name on the St Philippe juice trader"
    }
    CustomStringTableItem "{0C8AA12FDD414BE0}" {
     Id "EL-Resource_TradeJuice_Desc"
     Target_en_us "Sell apple juice for cash"
     Comment "Action description on the St Philippe juice trader"
    }
    CustomStringTableItem "{ACB4C4BD8DAAAA84}" {
     Id "EL-Rob_CiviliansOnly"
     Target_en_us "Only civilians can rob"
     Comment "Cash robbery refused because the player is not a civilian"
    }
    CustomStringTableItem "{F3C1408EE1332399}" {
     Id "EL-Rob_Cooldown"
     Target_en_us "The till was just robbed"
     Comment "Cash robbery refused because the cooldown is still running"
    }
    CustomStringTableItem "{B706E8A6FFF1E84A}" {
     Id "EL-Rob_NeedPolice"
     Target_en_us "Not enough {police} on duty"
     Comment "Cash robbery refused because too few police are on duty"
    }
  CustomStringTableItem "{0970D80CAA70B53F}" {
   Id "EL-Robbery_Failed"
   Target_en_us "Robbery Failed"
   Target_es_es "Robo Fallido"
   Target_de_de "Raub fehlgeschlagen"
   Target_fr_fr "Vol Échoué"
   Target_it_it "Rapina Fallita"
   Target_pt_pt "Roubo Falhado"
   Target_pt_br "Roubo Falhado"
  }
  CustomStringTableItem "{9B142C6279966BB8}" {
   Id "EL-Robbery_Successful"
   Target_en_us "Robbery Successful"
   Target_es_es "Robo Exitoso"
   Target_de_de "Raub erfolgreich"
   Target_fr_fr "Vol Réussi"
   Target_it_it "Rapina Riuscita"
   Target_pt_pt "Roubo Bem-Sucedido"
   Target_pt_br "Roubo Bem-Sucedido"
  }
    CustomStringTableItem "{F28F55445F43594B}" {
     Id "EL-SellBlocked_Equipped"
     Target_en_us "You cannot sell an equipped item"
     Target_de_de "Du kannst einen ausgerüsteten Gegenstand nicht verkaufen"
     Target_fr_fr "Vous ne pouvez pas vendre un objet équipé"
     Target_it_it "Non puoi vendere un oggetto equipaggiato"
     Target_es_es "No puedes vender un objeto equipado"
     Target_pt_pt "Não pode vender um item equipado"
     Target_pt_br "Você não pode vender um item equipado"
     Comment "Shop sell blocked: item is equipped"
    }
    CustomStringTableItem "{8C6C046D530707FD}" {
     Id "EL-SellBlocked_NoValue"
     Target_en_us "This item has no value"
     Target_de_de "Dieser Gegenstand hat keinen Wert"
     Target_fr_fr "Cet objet n'a aucune valeur"
     Target_it_it "Questo oggetto non ha valore"
     Target_es_es "Este objeto no tiene valor"
     Target_pt_pt "Este item não tem valor"
     Target_pt_br "Este item não tem valor"
     Comment "Shop sell blocked: item has no sell value"
    }
    CustomStringTableItem "{8C63A97A927C0B53}" {
     Id "EL-SellBlocked_NotBoughtHere"
     Target_en_us "This shop does not buy this item"
     Target_de_de "Dieses Geschäft kauft diesen Gegenstand nicht"
     Target_fr_fr "Ce magasin n'achète pas cet objet"
     Target_it_it "Questo negozio non compra questo oggetto"
     Target_es_es "Esta tienda no compra este objeto"
     Target_pt_pt "Esta loja não compra este item"
     Target_pt_br "Esta loja não compra este item"
     Comment "Shop sell blocked: shop does not buy this item"
    }
   CustomStringTableItem "{57624FF39CA0C9FB}" {
    Id "EL-Shop_Buy"
    Target_en_us "Buy"
    Target_de_de "Kaufen"
    Target_fr_fr "Acheter"
    Target_it_it "Compra"
    Target_es_es "Comprar"
    Target_pt_pt "Comprar"
    Target_pt_br "Comprar"
    Comment "Shop buy button"
   }
   CustomStringTableItem "{B1312D9A925343A1}" {
    Id "EL-Shop_Cash"
    Target_en_us "Cash"
    Target_de_de "Bargeld"
    Target_fr_fr "Liquide"
    Target_it_it "Contanti"
    Target_es_es "Efectivo"
    Target_pt_pt "Dinheiro"
    Target_pt_br "Dinheiro"
    Comment "Shop menu cash label"
   }
   CustomStringTableItem "{12501435C4030C81}" {
    Id "EL-Shop_PurchaseFailed"
    Target_en_us "Purchase failed"
    Target_de_de "Kauf fehlgeschlagen"
    Target_fr_fr "Achat échoué"
    Target_it_it "Acquisto non riuscito"
    Target_es_es "Compra fallida"
    Target_pt_pt "Compra falhou"
    Target_pt_br "Compra falhou"
    Comment "Shop purchase failure notification title"
   }
   CustomStringTableItem "{C5063E87653539D2}" {
    Id "EL-Shop_PurchaseSuccessful"
    Target_en_us "Purchase"
    Target_de_de "Kauf"
    Target_fr_fr "Achat"
    Target_it_it "Acquisto"
    Target_es_es "Compra"
    Target_pt_pt "Compra"
    Target_pt_br "Compra"
    Comment "Shop purchase notification title"
   }
CustomStringTableItem "{44869271883D0402}" {
    Id "EL-Shop_Results"
    Target_en_us "Showing %1 of %2 items"
    Target_de_de "%1 von %2 Artikeln angezeigt"
    Target_fr_fr "%1 articles sur %2 affichǸs"
    Target_it_it "Mostrati %1 di %2 articoli"
    Target_es_es "Mostrando %1 de %2 artículos"
    Target_pt_pt "A mostrar %1 de %2 artigos"
    Target_pt_br "Mostrando %1 de %2 artigos"
    Comment "Shop result-count hint"
   }
   CustomStringTableItem "{313F933C75272034}" {
    Id "EL-Shop_Search"
    Target_en_us "Search items"
    Target_de_de "Artikel suchen"
    Target_fr_fr "Rechercher des articles"
    Target_it_it "Cerca articoli"
    Target_es_es "Buscar artículos"
    Target_pt_pt "Pesquisar artigos"
    Target_pt_br "Pesquisar artigos"
    Comment "Shop search box placeholder"
   }
   CustomStringTableItem "{811696D359309A3D}" {
    Id "EL-Shop_Sell"
    Target_en_us "Sell"
    Target_de_de "Verkaufen"
    Target_fr_fr "Vendre"
    Target_it_it "Vendi"
    Target_es_es "Vender"
    Target_pt_pt "Vender"
    Target_pt_br "Vender"
    Comment "Shop sell button"
   }
   CustomStringTableItem "{1901F5D02C8E7AF1}" {
    Id "EL-Shop_SellFailed"
    Target_en_us "Nothing to sell"
    Target_de_de "Nichts zu verkaufen"
    Target_fr_fr "Rien à vendre"
    Target_it_it "Niente da vendere"
    Target_es_es "Nada que vender"
    Target_pt_pt "Nada para vender"
    Target_pt_br "Nada para vender"
    Comment "Shop sell failure notification body"
   }
  CustomStringTableItem "{4517233F8276C3B2}" {
    Id "EL-Shop_Title"
    Target_en_us "Shop"
    Target_de_de "Laden"
    Target_fr_fr "Magasin"
    Target_it_it "Negozio"
    Target_es_es "Tienda"
    Target_pt_pt "Loja"
    Target_pt_br "Loja"
    Comment "Shop menu title"
   }
  CustomStringTableItem "{4C73EC932CF638D8}" {
   Id "EL-ShowID_DisplayName"
   Target_en_us "Show ID"
   Target_es_es "Mostrar ID"
   Target_de_de "ID anzeigen"
   Target_fr_fr "Montrer l'ID"
   Target_it_it "Mostra ID"
   Target_pt_pt "Mostrar ID"
   Target_pt_br "Mostrar ID"
  }
   CustomStringTableItem "{41489243B750D903}" {
    Id "EL-SpawnLoc_Mine"
    Target_en_us "Mining Camp"
    Target_es_es "Campamento minero"
    Target_de_de "Minenlager"
    Target_fr_fr "Campement minier"
    Target_it_it "Campo minerario"
    Target_pt_pt "Campo mineiro"
    Target_pt_br "Acampamento de mineração"
   }
   CustomStringTableItem "{41489243B750D905}" {
    Id "EL-SpawnLoc_Outpost"
    Target_en_us "East Outpost"
    Target_es_es "Puesto del este"
    Target_de_de "Ost-Außenposten"
    Target_fr_fr "Avant-poste est"
    Target_it_it "Avamposto est"
    Target_pt_pt "Posto avançado este"
    Target_pt_br "Posto avançado leste"
   }
   CustomStringTableItem "{41489243B750D904}" {
    Id "EL-SpawnLoc_Station"
    Target_en_us "Police Station"
    Target_es_es "Comisaría"
    Target_de_de "Polizeiwache"
    Target_fr_fr "Poste de police"
    Target_it_it "Stazione di polizia"
    Target_pt_pt "Esquadra de polícia"
    Target_pt_br "Delegacia de polícia"
   }
   CustomStringTableItem "{41489243B750D901}" {
    Id "EL-SpawnLoc_Title"
    Target_en_us "Select Spawn Location"
    Target_es_es "Seleccionar punto de aparición"
    Target_de_de "Spawnpunkt wählen"
    Target_fr_fr "Choisir le point d'apparition"
    Target_it_it "Seleziona punto di comparsa"
    Target_pt_pt "Selecionar ponto de aparição"
    Target_pt_br "Selecionar ponto de surgimento"
   }
   CustomStringTableItem "{41489243B750D902}" {
    Id "EL-SpawnLoc_Town"
    Target_en_us "Town Square"
    Target_es_es "Plaza del pueblo"
    Target_de_de "Marktplatz"
    Target_fr_fr "Place du village"
    Target_it_it "Piazza del paese"
    Target_pt_pt "Praça da vila"
    Target_pt_br "Praça da vila"
   }
  CustomStringTableItem "{BA65E2A7D9A6EB16}" {
   Id "EL-Spawn_Failed"
   Target_en_us "Failed to spawn vehicle!"
   Target_es_es "Fallo al spawnear vehículo!"
   Target_de_de "Fahrzeug spawnen fehlgeschlagen!"
   Target_fr_fr "Échec du spawn de véhicule!"
   Target_it_it "Fallito spawn veicolo!"
   Target_pt_pt "Falha ao spawnar veículo!"
   Target_pt_br "Falha ao spawnar veículo!"
  }
  CustomStringTableItem "{5A7AC3DBDBC982B7}" {
   Id "EL-SplitQuantityDialog_Input_Amount"
   Target_en_us "Amount"
   Target_de_de "Anzahl"
   Target_fr_fr "Montant"
   Target_it_it "Importo"
   Target_es_es "Cantidad"
   Target_pt_pt "Quantidade"
   Target_pt_br "Quantidade"
   Modified 1517997640
   Author "Arkensor"
   LastChanged "Arkensor"
  }
  CustomStringTableItem "{5A7AC3DBD6ADFB48}" {
   Id "EL-SplitQuantityDialog_Input_Keep"
   Target_en_us "Keep"
   Target_de_de "Behalten"
   Target_fr_fr "Garder"
   Target_it_it "Mantieni"
   Target_es_es "Mantener"
   Target_pt_pt "Manter"
   Target_pt_br "Manter"
   Modified 1517997635
   Author "Arkensor"
   LastChanged "Arkensor"
  }
  CustomStringTableItem "{5A7AC3DBD51D98EF}" {
   Id "EL-SplitQuantityDialog_Input_SplitOff"
   Target_en_us "Split off"
   Target_de_de "Abteilen"
   Target_fr_fr "Séparer"
   Target_it_it "Separa"
   Target_es_es "Separar"
   Target_pt_pt "Separar"
   Target_pt_br "Separar"
   Modified 1517997613
   Author "Arkensor"
   LastChanged "Arkensor"
  }
  CustomStringTableItem "{5A7AC3DBDC021AB3}" {
   Id "EL-SplitQuantityDialog_Message"
   Target_en_us "Choose the stack size you want to split off"
   Target_de_de "Wähle die Größe des Stapels der abgespalten werden soll"
   Target_fr_fr "Choisissez la taille de la pile à séparer"
   Target_it_it "Scegli la dimensione della pila da separare"
   Target_es_es "Elige el tamaño de la pila que quieres separar"
   Target_pt_pt "Escolha o tamanho da pilha a separar"
   Target_pt_br "Escolha o tamanho da pilha a separar"
   Modified 1517997601
   Author "Arkensor"
   LastChanged "Arkensor"
  }
  CustomStringTableItem "{5A7AC3DBA1DFD8FD}" {
   Id "EL-SplitQuantityDialog_Name"
   Target_en_us "Split Stack"
   Target_de_de "Stapel aufteilen"
   Target_fr_fr "Diviser la pile"
   Target_it_it "Dividi pila"
   Target_es_es "Dividir Pila"
   Target_pt_pt "Dividir pilha"
   Target_pt_br "Dividir pilha"
   Modified 1517997561
   Author "Arkensor"
   LastChanged "Arkensor"
  }
  CustomStringTableItem "{51406463B7ACAB53}" {
   Id "EL-Stole_Money"
   Target_en_us "Stole {currency:sym}%1!"
   Target_es_es "Robó {currency:sym}%1!"
   Target_de_de "{currency:sym}%1 gestohlen!"
   Target_fr_fr "Volé {currency:sym}%1!"
   Target_it_it "Rubato {currency:sym}%1!"
   Target_pt_pt "Roubou {currency:sym}%1!"
   Target_pt_br "Roubou {currency:sym}%1!"
  }
    CustomStringTableItem "{E34C5F5AB19512D0}" {
     Id "EL-Survival_Health"
     Target_en_us "Health:"
     Target_de_de "Gesundheit:"
     Target_fr_fr "Santé :"
     Target_it_it "Salute:"
     Target_es_es "Salud:"
     Target_pt_pt "Saúde:"
     Target_pt_br "Saúde:"
     Comment "Survival HUD health label"
    }
    CustomStringTableItem "{F1A1819AC780B000}" {
     Id "EL-Survival_Hunger"
     Target_en_us "Hunger:"
     Target_de_de "Hunger:"
     Target_fr_fr "Faim :"
     Target_it_it "Fame:"
     Target_es_es "Hambre:"
     Target_pt_pt "Fome:"
     Target_pt_br "Fome:"
     Comment "Survival HUD hunger label"
    }
    CustomStringTableItem "{CCD6BB2C1A5D5FEC}" {
     Id "EL-Survival_Thirst"
     Target_en_us "Thirst:"
     Target_de_de "Durst:"
     Target_fr_fr "Soif :"
     Target_it_it "Sete:"
     Target_es_es "Sed:"
     Target_pt_pt "Sede:"
     Target_pt_br "Sede:"
     Comment "Survival HUD thirst label"
    }
  CustomStringTableItem "{589A396025738903}" {
   Id "EL-Trade_Successful"
   Target_en_us "Trade Successful"
   Target_es_es "Intercambio Exitoso"
   Target_de_de "Handel erfolgreich"
   Target_fr_fr "Échange Réussi"
   Target_it_it "Scambio Riuscito"
   Target_pt_pt "Troca Bem-Sucedida"
   Target_pt_br "Troca Bem-Sucedida"
  }
   CustomStringTableItem "{497094A09ECC498E}" {
    Id "EL-VehicleLock_Action"
    Target_en_us "Lock/Unlock Vehicle"
    Target_de_de "Fahrzeug ver-/entriegeln"
    Target_fr_fr "Verrouiller/Déverrouiller le véhicule"
    Target_it_it "Blocca/Sblocca veicolo"
    Target_es_es "Bloquear/Desbloquear vehículo"
    Target_pt_pt "Trancar/Destrancar veículo"
    Target_pt_br "Trancar/Destrancar veículo"
    Modified 1788019742
    Author "LifeFramework"
    LastChanged "LifeFramework"
   }
    CustomStringTableItem "{C5BBC2192BD8EFEC}" {
     Id "EL-VehicleLock_ActionLock"
     Target_en_us "Lock vehicle"
     Comment "Action label when the vehicle is unlocked (#412)."
    }
    CustomStringTableItem "{D6B5DE4ACCEECA33}" {
     Id "EL-VehicleLock_ActionUnlock"
     Target_en_us "Unlock vehicle"
     Comment "Action label when the vehicle is locked (#412)."
    }
   CustomStringTableItem "{3183B68A7D10419E}" {
    Id "EL-VehicleLock_MissingKey"
    Target_en_us "You need the key for this vehicle"
    Target_de_de "Sie benötigen den Schlüssel für dieses Fahrzeug"
    Target_fr_fr "Vous avez besoin de la clé de ce véhicule"
    Target_it_it "Ti serve la chiave di questo veicolo"
    Target_es_es "Necesitas la llave de este vehículo"
    Target_pt_pt "Precisa da chave deste veículo"
    Target_pt_br "Você precisa da chave deste veículo"
    Modified 1788019742
    Author "LifeFramework"
    LastChanged "LifeFramework"
   }
   CustomStringTableItem "{AEBA171181D64DD6}" {
    Id "EL-VehicleLock_Reason"
    Target_en_us "Vehicle is locked"
    Target_de_de "Fahrzeug ist abgeschlossen"
    Target_fr_fr "Véhicule verrouillé"
    Target_it_it "Veicolo bloccato"
    Target_es_es "Vehículo bloqueado"
    Target_pt_pt "Veículo trancado"
    Target_pt_br "Veículo trancado"
    Modified 1788019742
    Author "LifeFramework"
    LastChanged "LifeFramework"
   }
   CustomStringTableItem "{863E7B439E7E4BDF}" {
    Id "EL-VehicleLock_Title"
    Target_en_us "Vehicle Lock"
    Target_de_de "Fahrzeugschloss"
    Target_fr_fr "Verrou du véhicule"
    Target_it_it "Blocco veicolo"
    Target_es_es "Bloqueo de vehículo"
    Target_pt_pt "Bloqueio do veículo"
    Target_pt_br "Bloqueio do veículo"
    Modified 1788019742
    Author "LifeFramework"
    LastChanged "LifeFramework"
   }
   CustomStringTableItem "{AB000BA2CB254D71}" {
    Id "EL-VehicleLocked"
    Target_en_us "Your vehicle is now locked."
    Target_de_de "Ihr Fahrzeug ist jetzt abgeschlossen."
    Target_fr_fr "Votre véhicule est maintenant verrouillé."
    Target_it_it "Il tuo veicolo è ora bloccato."
    Target_es_es "Tu vehículo ahora está bloqueado."
    Target_pt_pt "O seu veículo agora está trancado."
    Target_pt_br "Seu veículo agora está trancado."
    Modified 1788019742
    Author "LifeFramework"
    LastChanged "LifeFramework"
   }
   CustomStringTableItem "{A20CA0D7DF1C4CED}" {
    Id "EL-VehicleUnlocked"
    Target_en_us "Your vehicle is now unlocked."
    Target_de_de "Ihr Fahrzeug ist jetzt aufgeschlossen."
    Target_fr_fr "Votre véhicule est maintenant déverrouillé."
    Target_it_it "Il tuo veicolo è ora sbloccato."
    Target_es_es "Tu vehículo ahora está desbloqueado."
    Target_pt_pt "O seu veículo agora está destrancado."
    Target_pt_br "Seu veículo agora está destrancado."
    Modified 1788019742
    Author "LifeFramework"
    LastChanged "LifeFramework"
   }
    CustomStringTableItem "{E1C0E0010000000C}" {
     Id "EL-Vehicle_Bought"
     Target_en_us "Vehicle purchased"
     Comment "Vehicle lot purchase succeeded"
    }
    CustomStringTableItem "{E1C0E00100000001}" {
     Id "EL-Vehicle_Buy"
     Target_en_us "Buy vehicle"
     Comment "Vehicle lot buy action"
    }
    CustomStringTableItem "{E1C0E00100000006}" {
     Id "EL-Vehicle_CannotAfford"
     Target_en_us "You cannot afford this vehicle"
     Comment "Vehicle purchase refused because cash or the ATM balance is short"
    }
    CustomStringTableItem "{E1C0E00100000F01}" {
     Id "EL-Vehicle_Dana2101"
     Target_en_us "Dana 2101"
     Comment "Lot catalog row for the beige Dana 2101 (vanilla V2101, free in 1.9)"
    }
    CustomStringTableItem "{E1C0E00100000002}" {
     Id "EL-Vehicle_ExtraKey"
     Target_en_us "Cut an extra key"
     Comment "Owner action that cuts another key for an owned vehicle"
    }
    CustomStringTableItem "{E1C0E0010000000B}" {
     Id "EL-Vehicle_M923"
     Target_en_us "Cargo truck"
     Comment "Lot catalog row for the M923 cargo truck"
    }
    CustomStringTableItem "{E1C0E0010000000D}" {
     Id "EL-Vehicle_NoSpawn"
     Target_en_us "The lot has nowhere to put that vehicle"
     Comment "Vehicle spawn refused because every pad is occupied"
    }
    CustomStringTableItem "{E1C0E00100000005}" {
     Id "EL-Vehicle_NotOwner"
     Target_en_us "You do not own this vehicle"
     Comment "Vehicle action refused because the player is not the owner"
    }
    CustomStringTableItem "{E1C0E0010000000E}" {
     Id "EL-Vehicle_NotSold"
     Target_en_us "This lot does not sell that vehicle"
     Comment "Vehicle purchase refused because the prefab is not in the catalog"
    }
    CustomStringTableItem "{E1C0E00100000004}" {
     Id "EL-Vehicle_Retrieve"
     Target_en_us "Retrieve vehicle"
     Comment "Garage action that spawns a stored vehicle"
    }
    CustomStringTableItem "{E1C0E00100000007}" {
     Id "EL-Vehicle_S105"
     Target_en_us "S105"
     Comment "Lot catalog row for the beige S105"
    }
    CustomStringTableItem "{E1C0E00100000009}" {
     Id "EL-Vehicle_S1203"
     Target_en_us "Van"
     Comment "Lot catalog row for the beige S1203"
    }
    CustomStringTableItem "{E1C0E00100000F02}" {
     Id "EL-Vehicle_SputnikT601"
     Target_en_us "Sputnik T601"
     Comment "Lot catalog row for the beige Sputnik T601 (vanilla T601, free in 1.9)"
    }
  CustomStringTableItem "{769912DD3060E56A}" {
   Id "EL-Vehicle_Stolen"
   Target_en_us "Vehicle stolen!"
   Target_es_es "Vehículo robado!"
   Target_de_de "Fahrzeug gestohlen!"
   Target_fr_fr "Véhicule volé!"
   Target_it_it "Veicolo rubato!"
   Target_pt_pt "Veículo roubado!"
   Target_pt_br "Veículo roubado!"
  }
    CustomStringTableItem "{E1C0E00100000003}" {
     Id "EL-Vehicle_Store"
     Target_en_us "Store vehicle"
     Comment "Garage action that stores the owner's vehicle"
    }
    CustomStringTableItem "{E1C0E0010000000A}" {
     Id "EL-Vehicle_UAZ452"
     Target_en_us "UAZ 452"
     Comment "Lot catalog row for the UAZ452"
    }
    CustomStringTableItem "{E1C0E00100000008}" {
     Id "EL-Vehicle_UAZ469"
     Target_en_us "UAZ"
     Comment "Lot catalog row for the covered civilian UAZ"
    }
  CustomStringTableItem "{D8E4F8A5027A13A8}" {
   Id "EL-Weapon_Stolen"
   Target_en_us "Weapon stolen!"
   Target_es_es "Arma robada!"
   Target_de_de "Waffe gestohlen!"
   Target_fr_fr "Arme volée!"
   Target_it_it "Arma rubata!"
   Target_pt_pt "Arma roubada!"
   Target_pt_br "Arma roubada!"
  }
    CustomStringTableItem "{71B1EFBEBC90D63B}" {
     Id "EL-Whitelist_NotAllowed"
     Target_en_us "You are not allowed to use this"
     Target_de_de "Du darfst dies nicht verwenden"
     Target_fr_fr "Vous n'êtes pas autorisé à utiliser ceci"
     Target_it_it "Non sei autorizzato a usarlo"
     Target_es_es "No tienes permitido usar esto"
     Target_pt_pt "Não está autorizado a usar isto"
     Target_pt_br "Você não está autorizado a usar isto"
     Comment "Whitelist-gated action cannot-perform reason"
    }
    CustomStringTableItem "{11FBC6AD34589936}" {
     Id "EL-Whitelist_Sign_Action"
     Target_en_us "Whitelist Sign"
     Target_de_de "Whitelist-Schild"
     Target_fr_fr "Panneau de liste blanche"
     Target_it_it "Insegna whitelist"
     Target_es_es "Cartel de lista blanca"
     Target_pt_pt "Placa de lista de permissões"
     Target_pt_br "Placa de lista de permissões"
     Comment "Whitelist sign interactive action name"
    }
    CustomStringTableItem "{AA8EFD68EBC44F28}" {
     Id "LF-ATMTransfer_Done"
     Target_en_us "SENT %1 TO ...%2. BALANCE %3."
     Comment "ATM transfer page status line after a transfer. %1 amount, %2 last four characters of the recipient's number, %3 the new balance."
    }
    CustomStringTableItem "{8359F0DA9137A580}" {
     Id "LF-ATMTransfer_FieldTarget"
     Target_en_us "SEND TO ACCOUNT NUMBER"
     Comment "ATM transfer page: caption over the box where the recipient's account number is typed."
    }
    CustomStringTableItem "{B07FF790CDDE54BC}" {
     Id "LF-ATMTransfer_Header"
     Target_en_us "TRANSFER"
     Comment "ATM transfer page title."
    }
    CustomStringTableItem "{6FDD215150BDD14B}" {
     Id "LF-ATMTransfer_KeyMenu"
     Target_en_us "MENU"
     Comment "ATM transfer page: key back to the terminal's main screen."
    }
    CustomStringTableItem "{C08EF8EC1F48F181}" {
     Id "LF-ATMTransfer_KeySend"
     Target_en_us "SEND"
     Comment "ATM transfer page: confirm key."
    }
    CustomStringTableItem "{6CAF1AF5B5009021}" {
     Id "LF-ATMTransfer_LineAmount"
     Target_en_us "AMOUNT > %1_"
     Comment "ATM transfer page entry line with a cursor. %1 is the amount keyed in so far."
    }
    CustomStringTableItem "{7E5731E1EBAE9D1A}" {
     Id "LF-ATMTransfer_LineFee"
     Target_en_us "FEE %1  TOTAL %2"
     Comment "ATM transfer page: the fee for the amount keyed in and what leaves the account. %1 fee, %2 total."
    }
    CustomStringTableItem "{FC75BE37C547EB67}" {
     Id "LF-ATMTransfer_LineOwn"
     Target_en_us "YOUR NUMBER: %1"
     Comment "ATM transfer page: the player's own account number, to tell someone. %1 is the number."
    }
    CustomStringTableItem "{6005A6BA2670AB55}" {
     Id "LF-ATMTransfer_Prompt"
     Target_en_us "KEY IN THE NUMBER AND AMOUNT, THEN SEND."
     Comment "ATM transfer page status line while idle."
    }
    CustomStringTableItem "{D2E85BF67E21E2DC}" {
     Id "LF-ATMTransfer_RefuseFailed"
     Target_en_us "THE TRANSFER DID NOT GO THROUGH. NO MONEY MOVED."
     Comment "ATM transfer page refusal: the transfer id was used or the bank refused for a reason the player cannot fix."
    }
    CustomStringTableItem "{B77B1450A1912D0E}" {
     Id "LF-ATMTransfer_RefuseRate"
     Target_en_us "SLOW DOWN. TRY AGAIN IN A MOMENT."
     Comment "ATM transfer page refusal: too many requests from this player in a few seconds."
    }
    CustomStringTableItem "{B49AD51FE0064222}" {
     Id "LF-ATMTransfer_Sent"
     Target_en_us "You sent %1 to the account ending %2."
     Comment "Toast after a bank transfer. %1 is the amount, %2 the last four characters of the recipient's number."
    }
    CustomStringTableItem "{EA0C8B8F73FBDA3F}" {
     Id "LF-ATMTransfer_ToastTitle"
     Target_en_us "Bank transfer"
     Comment "Toast title after a bank transfer went through."
    }
    CustomStringTableItem "{4A64CBDDC70A3B14}" {
     Id "LF-ATM_Deposited"
     Target_en_us "DEPOSITED %1"
     Comment "ATM terminal status line after a deposit. %1 is the amount."
    }
    CustomStringTableItem "{CAF4A9272F7A5651}" {
     Id "LF-ATM_Dispensed"
     Target_en_us "TAKE CASH %1"
     Comment "ATM terminal status line after a withdrawal. %1 is the amount paid out."
    }
    CustomStringTableItem "{2028FAF6D9D3BF5C}" {
     Id "LF-ATM_Exit"
     Target_en_us "EXIT"
     Comment "ATM terminal key that closes the screen."
    }
    CustomStringTableItem "{E5BDB7CD47972752}" {
     Id "LF-ATM_Header"
     Target_en_us "FIRST BANK"
     Comment "ATM terminal screen header. Upper case: a 1989 green-screen terminal."
    }
    CustomStringTableItem "{A67CBEC8AAD45E67}" {
     Id "LF-ATM_KeyBack"
     Target_en_us "DEL"
     Comment "ATM keypad key that removes the last digit."
    }
    CustomStringTableItem "{9549498E04A2B4B9}" {
     Id "LF-ATM_KeyClear"
     Target_en_us "CLR"
     Comment "ATM keypad key that clears the amount."
    }
    CustomStringTableItem "{B052CECB9FD10BBA}" {
     Id "LF-ATM_KeyDeposit"
     Target_en_us "DEPOSIT"
     Comment "ATM terminal key: pay the keyed amount of carried cash into the account."
    }
    CustomStringTableItem "{ACF0AF7CCD90F1B7}" {
     Id "LF-ATM_KeyTransfer"
     Target_en_us "TRANSFER"
     Comment "ATM terminal key that opens the transfer page."
    }
    CustomStringTableItem "{9BA4937AFAD95E3E}" {
     Id "LF-ATM_KeyWithdraw"
     Target_en_us "WITHDRAW"
     Comment "ATM terminal key: take the keyed amount out of the account as cash."
    }
    CustomStringTableItem "{627D8D9DEE75887B}" {
     Id "LF-ATM_LineBalance"
     Target_en_us "BALANCE ....... %1"
     Comment "ATM terminal balance line. %1 is the formatted account balance, or ---- before the bank answers."
    }
    CustomStringTableItem "{78E20ABFCFFBB92B}" {
     Id "LF-ATM_LineCash"
     Target_en_us "CASH IN HAND .. %1"
     Comment "ATM terminal line for the cash the player carries. %1 is the formatted amount."
    }
    CustomStringTableItem "{2C6232410D2F89DE}" {
     Id "LF-ATM_LineDebt"
     Target_en_us "UNPAID DEBT ... %1"
     Comment "ATM terminal line for hospital bills past their deadline, taken from the bank as it fills. %1 is the formatted debt. Hidden when nothing is owed."
    }
    CustomStringTableItem "{7F695CCFB7B1DEFD}" {
     Id "LF-ATM_LineEntry"
     Target_en_us "AMOUNT > %1_"
     Comment "ATM terminal entry line with a cursor. %1 is the amount keyed in so far."
    }
    CustomStringTableItem "{6B1D3E9A0C4F7285}" {
     Id "LF-ATM_Manage"
     Target_en_us "Manage bank account"
     Comment "Action description on a placed ATM (LF_ATM.et)."
    }
    CustomStringTableItem "{7880A378781D2C8D}" {
     Id "LF-ATM_NoReply"
     Target_en_us "NO REPLY. RETRY"
     Comment "ATM terminal status line when the server did not answer within 5 seconds."
    }
    CustomStringTableItem "{C69157C8E92DD27F}" {
     Id "LF-ATM_Prompt"
     Target_en_us "KEY IN AN AMOUNT, THEN DEPOSIT OR WITHDRAW"
     Comment "ATM terminal status line when idle."
    }
    CustomStringTableItem "{8EB7067ED400B203}" {
     Id "LF-ATM_QuickBalance"
     Target_en_us "Check balance"
     Comment "ATM screen and keypad line: show the balance."
    }
    CustomStringTableItem "{55BAFAE9BB29A9AB}" {
     Id "LF-ATM_QuickCannotAct"
     Target_en_us "Your hands are not free"
     Comment "Why ATM lines are greyed: dead, downed or cuffed."
    }
    CustomStringTableItem "{3DFE09BD44E660F8}" {
     Id "LF-ATM_QuickDeposit"
     Target_en_us "Deposit %1"
     Comment "ATM deposit drawer line. %1 = the amount."
    }
    CustomStringTableItem "{A067E84B7AE4FD1F}" {
     Id "LF-ATM_QuickDepositAll"
     Target_en_us "Deposit all cash (%1)"
     Comment "ATM deposit drawer line: every note carried. %1 = that amount."
    }
    CustomStringTableItem "{DED7547D25B8045D}" {
     Id "LF-ATM_QuickDepositAllPlain"
     Target_en_us "Deposit all cash"
     Comment "ATM deposit drawer line before the cash is known."
    }
    CustomStringTableItem "{8E750AA3EE40D7B3}" {
     Id "LF-ATM_QuickNoBalance"
     Target_en_us "Not enough in the bank"
     Comment "Why an ATM withdraw line is greyed."
    }
    CustomStringTableItem "{7DBB5D014D923E13}" {
     Id "LF-ATM_QuickNoCash"
     Target_en_us "Not enough cash on you"
     Comment "Why an ATM deposit line is greyed."
    }
    CustomStringTableItem "{11964E8003A3B483}" {
     Id "LF-ATM_QuickOverLimit"
     Target_en_us "Over the machine limit"
     Comment "Why an ATM line is greyed: above the single-transaction limit."
    }
    CustomStringTableItem "{1F77DAC6D39BBF60}" {
     Id "LF-ATM_QuickReading"
     Target_en_us "Reading your balance"
     Comment "Why an ATM 'all' line is greyed for a moment on arrival."
    }
    CustomStringTableItem "{95DB60E5BBB58114}" {
     Id "LF-ATM_QuickTransfer"
     Target_en_us "Bank transfer"
     Comment "ATM keypad line: open the transfer page."
    }
    CustomStringTableItem "{EFCE03C5109AA40C}" {
     Id "LF-ATM_QuickWithdraw"
     Target_en_us "Withdraw %1"
     Comment "ATM cash slot line. %1 = the amount."
    }
    CustomStringTableItem "{23550C5C06BBA898}" {
     Id "LF-ATM_QuickWithdrawAll"
     Target_en_us "Withdraw all (%1)"
     Comment "ATM cash slot line: the whole balance. %1 = that amount."
    }
    CustomStringTableItem "{C3F4D1C5361502B7}" {
     Id "LF-ATM_QuickWithdrawAllPlain"
     Target_en_us "Withdraw all"
     Comment "ATM cash slot line before the balance is known."
    }
    CustomStringTableItem "{74042C8BB24E2579}" {
     Id "LF-ATM_QuickWorking"
     Target_en_us "Machine working"
     Comment "Why ATM lines are greyed while a request is on its way."
    }
    CustomStringTableItem "{234C990779F19F26}" {
     Id "LF-ATM_RefuseAccount"
     Target_en_us "ACCOUNT ERROR"
     Comment "ATM refusal: the player's account could not be found."
    }
    CustomStringTableItem "{8839595C4DED196B}" {
     Id "LF-ATM_RefuseAmount"
     Target_en_us "CHECK AMOUNT"
     Comment "ATM refusal: the amount is zero or outside the transaction limit."
    }
    CustomStringTableItem "{FA6C0202EB152BE3}" {
     Id "LF-ATM_RefuseBalance"
     Target_en_us "NOT ENOUGH FUNDS"
     Comment "ATM refusal: a withdrawal larger than the account balance."
    }
    CustomStringTableItem "{B166036CBE991244}" {
     Id "LF-ATM_RefuseCannotAct"
     Target_en_us "HANDS NOT FREE"
     Comment "ATM refusal: the player is dead, downed or cuffed."
    }
    CustomStringTableItem "{4FE6F21A50365940}" {
     Id "LF-ATM_RefuseCash"
     Target_en_us "NOT ENOUGH CASH"
     Comment "ATM refusal: a deposit larger than the cash the player carries."
    }
    CustomStringTableItem "{2FA569A76F73369A}" {
     Id "LF-ATM_RefuseFar"
     Target_en_us "STAND AT MACHINE"
     Comment "ATM refusal: the server found no ATM within 5 m of the player."
    }
    CustomStringTableItem "{9AC0372760E9657E}" {
     Id "LF-ATM_RefusePockets"
     Target_en_us "POCKETS FULL"
     Comment "ATM refusal: the withdrawn cash does not fit in the inventory; the balance is restored."
    }
    CustomStringTableItem "{2B2DD7DA486A3225}" {
     Id "LF-ATM_ScreenBank"
     Target_en_us "BANK  %1"
     Comment "ATM screen line drawn on the machine for the player at it. %1 = bank balance."
    }
    CustomStringTableItem "{A52D49E2D7C6ACAC}" {
     Id "LF-ATM_ScreenCash"
     Target_en_us "CASH  %1"
     Comment "ATM screen line drawn on the machine for the player at it. %1 = cash carried."
    }
    CustomStringTableItem "{2C48CC14A874751E}" {
     Id "LF-ATM_ScreenHello"
     Target_en_us "LOOK AT A SLOT"
     Comment "ATM screen status line when the player walks up: look at the cash slot or deposit drawer."
    }
    CustomStringTableItem "{091A87E9815898A1}" {
     Id "LF-ATM_ToastBalance"
     Target_en_us "Bank %1. Cash %2"
     Comment "Toast for the ATM's Check balance. %1 = balance, %2 = cash carried."
    }
    CustomStringTableItem "{9AD68281491E7D15}" {
     Id "LF-ATM_ToastDeposited"
     Target_en_us "Deposited %1. Bank %2"
     Comment "Toast after an ATM deposit. %1 = amount, %2 = new balance."
    }
    CustomStringTableItem "{C15EE5CA1CDE1A4F}" {
     Id "LF-ATM_ToastWithdrew"
     Target_en_us "Withdrew %1. Bank %2"
     Comment "Toast after an ATM withdrawal. %1 = amount, %2 = new balance."
    }
    CustomStringTableItem "{E54FB6AD91A6A823}" {
     Id "LF-ATM_Unknown"
     Target_en_us "----"
     Comment "Placeholder for a figure the bank has not sent yet."
    }
    CustomStringTableItem "{C537821836A5ADD5}" {
     Id "LF-ATM_Working"
     Target_en_us "PLEASE WAIT ..."
     Comment "ATM terminal status line while the server handles a request."
    }
    CustomStringTableItem "{6AFAA16B46ED882D}" {
     Id "LF-Activity_FactsDone"
     Target_en_us "You know the way now. Keep at it to level up."
     Comment "Next tab line for a done activity."
    }
    CustomStringTableItem "{414E495D4669B3BB}" {
     Id "LF-Activity_FactsStep"
     Target_en_us "Step %1 of %2: %3"
     Comment "Next tab line for a running activity. %1 step number, %2 step count, %3 the step's title."
    }
    CustomStringTableItem "{C70A4369B3A513B9}" {
     Id "LF-Activity_FactsSteps"
     Target_en_us "%1 short steps, each with a waypoint."
     Comment "Next tab line for an activity not started. %1 is the number of steps."
    }
    CustomStringTableItem "{6E9D97E2C0EE30C6}" {
     Id "LF-Activity_Guide"
     Target_en_us "Resume"
     Comment "Next tab button on a running activity: put the waypoint and the step note back on its current step."
    }
    CustomStringTableItem "{917EF52F3C24A83B}" {
     Id "LF-Activity_Guided"
     Target_en_us "The waypoint is back on your current step."
     Comment "Next tab line after Resume."
    }
    CustomStringTableItem "{366A7B92337FE227}" {
     Id "LF-Activity_Kilometres"
     Target_en_us "%1 km"
     Comment "Next tab distance. %1 is kilometres with one decimal."
    }
    CustomStringTableItem "{3C9143AD27977EBB}" {
     Id "LF-Activity_Metres"
     Target_en_us "%1 m"
     Comment "Next tab distance under a kilometre. %1 is metres."
    }
    CustomStringTableItem "{2AAF598BE0DFACAE}" {
     Id "LF-Activity_NeedPapers"
     Target_en_us "First get your papers from the clerk: the waypoint shows the way. Work opens up after that."
     Comment "Next tab line for a newcomer who has not been registered by the clerk yet: no run can start."
    }
    CustomStringTableItem "{3471AB6ECF42E732}" {
     Id "LF-Activity_None"
     Target_en_us "Nothing new to start right now. Check back after your next level."
     Comment "Next tab line when no activity is open to the player."
    }
    CustomStringTableItem "{D0DAF9C307AB6A0D}" {
     Id "LF-Activity_PayDistance"
     Target_en_us "%1 · %2"
     Comment "Next tab row status. %1 is the pay an hour, %2 the distance."
    }
    CustomStringTableItem "{16B4C404FE5B8ED1}" {
     Id "LF-Activity_PayHour"
     Target_en_us "%1/h"
     Comment "Next tab row status. %1 is money, e.g. $470."
    }
    CustomStringTableItem "{7E1A4C93D05B2F68}" {
     Id "LF-Activity_PayNone"
     Target_en_us "no wage"
     Comment "Next tab row status for a row that pays nothing by the hour (founding a crew); it is a first step, not a job."
    }
    CustomStringTableItem "{4835914B88D22934}" {
     Id "LF-Activity_PayUnknown"
     Target_en_us "pay varies"
     Comment "Next tab row status when the pay an hour is not known."
    }
    CustomStringTableItem "{2A700326525E7C6D}" {
     Id "LF-Activity_PickedPitch"
     Target_en_us "%1: %2"
     Comment "Next tab detail line: the picked run and its pitch. %1 = run name, %2 = pitch."
    }
    CustomStringTableItem "{BC67C95738DDED4A}" {
     Id "LF-Activity_Refused"
     Target_en_us "That cannot start right now."
     Comment "Next tab line when the server refused Start."
    }
    CustomStringTableItem "{AF249A8804DAF7AF}" {
     Id "LF-Activity_RowStarted"
     Target_en_us "%1 · in progress"
     Comment "Next tab row status of a running activity. %1 is pay and distance."
    }
    CustomStringTableItem "{5B14E9465D155E95}" {
     Id "LF-Activity_Start"
     Target_en_us "Start"
     Comment "Next tab button: begin the picked activity's guided first steps."
    }
    CustomStringTableItem "{9A24A1CE4D148E68}" {
     Id "LF-Activity_Started"
     Target_en_us "Started. Follow the waypoint on your screen and map."
     Comment "Next tab line after Start was accepted."
    }
    CustomStringTableItem "{B94F998107F25B04}" {
     Id "LF-Activity_StateDone"
     Target_en_us "Done"
     Comment "Next tab: the activity's guided steps are all done."
    }
    CustomStringTableItem "{58A349D528905815}" {
     Id "LF-Activity_Summary"
     Target_en_us "Ways to earn a living, and first steps to take (a row marked no wage pays nothing). Pick one and press Start: each step puts a waypoint on your map."
     Comment "Next tab line above the activity list."
    }
    CustomStringTableItem "{51D3548DA687534E}" {
     Id "LF-Activity_Tab"
     Target_en_us "Next"
     Comment "Player menu tab label: what to do next (the activities the player can start)."
    }
    CustomStringTableItem "{E073897869DE9A6D}" {
     Id "LF-Activity_Title"
     Target_en_us "PERSONAL FILE · WHAT TO DO NEXT"
     Comment "Player menu title on the next tab."
    }
    CustomStringTableItem "{E4BA7BB0A0BF3887}" {
     Id "LF-Activity_Waiting"
     Target_en_us "Looking up what you can do..."
     Comment "Next tab line while the server's answer is on its way."
    }
    CustomStringTableItem "{5E1B7A39C2D40F68}" {
     Id "LF-Arrival_ClerkMark"
     Target_en_us "The registry clerk"
     Comment "Name on the waypoint and the map mark that guide a new player to the registry clerk (the arrival chain, first and last step). 32 characters at most."
    }
    CustomStringTableItem "{EA5E950371F7CE44}" {
     Id "LF-Atmosphere_SwitchOff"
     Target_en_us "Switch off"
     Comment "Action on a radio that is playing music (Feature/Atmosphere)."
    }
    CustomStringTableItem "{C1C1995B07DB81F6}" {
     Id "LF-Atmosphere_SwitchOn"
     Target_en_us "Switch on"
     Comment "Action on a radio that is switched off (Feature/Atmosphere)."
    }
    CustomStringTableItem "{7B1E5A0C00000005}" {
     Id "LF-BackRoom_AlreadyShut"
     Target_en_us "Already searched. The room is shut."
     Comment "Toast: the back room was raided recently (#600)."
    }
    CustomStringTableItem "{74C05AD0FF1A649C}" {
     Id "LF-BackRoom_DoorShut"
     Target_en_us "Back room (shut)"
     Comment "Prompt on the back room's counter while it is shut after a police search (#933)."
    }
    CustomStringTableItem "{7B1E5A0C00000002}" {
     Id "LF-BackRoom_Search"
     Target_en_us "Search back room"
     Comment "Officer action on a back room: search it and seize its stock as evidence (#600)."
    }
    CustomStringTableItem "{7B1E5A0C00000003}" {
     Id "LF-BackRoom_SearchDesc"
     Target_en_us "Seize the stock as evidence"
     Comment "Description of the back room search action (#600)."
    }
    CustomStringTableItem "{7B1E5A0C00000004}" {
     Id "LF-BackRoom_Seized"
     Target_en_us "Stock seized as evidence"
     Comment "Toast: the back room search seized its stock (#600)."
    }
    CustomStringTableItem "{2E7A4C1B90D8635F}" {
     Id "LF-Balaclava_Description"
     Target_en_us "A black knitted balaclava with a single eye opening. Anyone wearing it can't be recognized by their face."
     Comment "Headgear item description (tools/assets/balaclava, #1997)"
    }
    CustomStringTableItem "{6B2D90E4A1C37F58}" {
     Id "LF-Balaclava_Name"
     Target_en_us "Balaclava"
     Comment "Headgear item name (tools/assets/balaclava, #1997)"
    }
    CustomStringTableItem "{A27300000000BA02}" {
     Id "LF-BankKit_Take"
     Target_en_us "Take bank robbery kit"
     Comment "Playtest kit stand beside the bank: hands out a drill, keycard and charge (#605). Debug game mode only."
    }
    CustomStringTableItem "{45B885CB207E6AFA}" {
     Id "LF-BankTransfer_RefuseAmount"
     Target_en_us "Enter an amount above zero."
     Comment "Bank transfer refusal: the amount is zero or negative."
    }
    CustomStringTableItem "{095535993A8D1611}" {
     Id "LF-BankTransfer_RefuseBalance"
     Target_en_us "Your balance doesn't cover the amount and the fee."
     Comment "Bank transfer refusal: the balance is short of the amount plus the fee."
    }
    CustomStringTableItem "{EED4BC3488E3080F}" {
     Id "LF-BankTransfer_RefuseLimit"
     Target_en_us "That would pass today's transfer limit."
     Comment "Bank transfer refusal: the amount would take the sender past the daily limit."
    }
    CustomStringTableItem "{65E7F78A397137B3}" {
     Id "LF-BankTransfer_RefuseOwn"
     Target_en_us "You can't transfer money to your own account."
     Comment "Bank transfer refusal: the account number keyed in is the sender's own."
    }
    CustomStringTableItem "{2D4831EC50F493F3}" {
     Id "LF-BankTransfer_RefuseUnknown"
     Target_en_us "There is no account with that number."
     Comment "Bank transfer refusal: no account holds the number keyed in."
    }
    CustomStringTableItem "{A3AB2264A6CAAA6D}" {
     Id "LF-Bank_Drill"
     Target_en_us "Drill the vault door"
     Comment "Vault prompt while the drill is held in the hands (hand tool)."
    }
    CustomStringTableItem "{CAA5DA4BC5610FC2}" {
     Id "LF-Bank_DrillBusy"
     Target_en_us "Someone is already drilling this vault"
     Comment "Vault refusal: another robber's drill is running."
    }
    CustomStringTableItem "{73C6A60BDEBFFA84}" {
     Id "LF-Bank_DrillStarted"
     Target_en_us "Drilling. Hold the drill at the door until the lock gives."
     Comment "Notice when the timed vault drill starts."
    }
    CustomStringTableItem "{E6126385C63EE88F}" {
     Id "LF-Bank_DrillStop"
     Target_en_us "Stop drilling"
     Comment "Vault prompt for the robber who is drilling; cancels and pays nothing."
    }
    CustomStringTableItem "{5689C202D23903C6}" {
     Id "LF-Bank_DrillStopped"
     Target_en_us "The drilling stopped. Nothing gave."
     Comment "Notice when the vault drill is cancelled, interrupted, unequipped or walked away from."
    }
    CustomStringTableItem "{05B9191F30E7B9A2}" {
     Id "LF-Bank_HoldDrill"
     Target_en_us "Take the drill in your hands (quick slot) to drill the vault"
     Comment "Vault refusal: the drill is carried but not held."
    }
    CustomStringTableItem "{A27300000000BA01}" {
     Id "LF-Bank_NeedTool"
     Target_en_us "The vault needs a drill or a charge"
     Comment "Vault robbery refused: the robber carries none of the vault's fence tools (#273)"
    }
    CustomStringTableItem "{4B1E7A2C9D3F4081}" {
     Id "LF-Bank_Teller_Talk"
     Target_en_us "Talk to the teller"
     Comment "Prompt on the bank teller at the counter window. Opens the deposit and withdraw screen."
    }
    CustomStringTableItem "{7C20D5E8A1B94F36}" {
     Id "LF-Bank_Teller_TalkDesc"
     Target_en_us "Deposit or withdraw cash at the counter"
     Comment "Description of the bank teller prompt."
    }
    CustomStringTableItem "{62D75ACB54C1AA09}" {
     Id "LF-Barber_Action"
     Target_en_us "Ask for a haircut"
     Comment "Action while seated on a barber chair: open the barber sheet (#3234)."
    }
    CustomStringTableItem "{B9315E3528BAE1E3}" {
     Id "LF-Barber_Asking"
     Target_en_us "The barber gets to work..."
     Comment "Barber sheet status while the server answers (#3234)."
    }
    CustomStringTableItem "{FE9688F31B4E6799}" {
     Id "LF-Barber_Close"
     Target_en_us "Leave the chair as you were"
     Comment "Barber sheet close button; the old look comes back (#3234)."
    }
    CustomStringTableItem "{AFE2CDD8B76005A5}" {
     Id "LF-Barber_Done"
     Target_en_us "Paid. Enjoy the new look."
     Comment "Barber sheet status after a paid haircut (#3234)."
    }
    CustomStringTableItem "{8DC241E8A6EA978A}" {
     Id "LF-Barber_Intro"
     Target_en_us "Pick a new hair and beard, then pay at the chair."
     Comment "First line of the barber sheet (#3234)."
    }
    CustomStringTableItem "{BCEF22626AE37FE8}" {
     Id "LF-Barber_Look"
     Target_en_us "Look %1 of %2"
     Comment "Barber sheet: which look is shown on the seated character. %1 the look, %2 how many (#3234)."
    }
    CustomStringTableItem "{6DAE9AB8265EE308}" {
     Id "LF-Barber_Next"
     Target_en_us "Next look"
     Comment "Barber sheet button (#3234)."
    }
    CustomStringTableItem "{5CE95C8E08A13CA8}" {
     Id "LF-Barber_Pay"
     Target_en_us "Pay {currency:sym}%1 for this look"
     Comment "Barber sheet button: pay the fee and keep the shown look. %1 the fee in cash (#3234)."
    }
    CustomStringTableItem "{7721BE3239E4E078}" {
     Id "LF-Barber_Prev"
     Target_en_us "Previous look"
     Comment "Barber sheet button (#3234)."
    }
    CustomStringTableItem "{63094ABE99436B00}" {
     Id "LF-Barber_RefuseNoCash"
     Target_en_us "Not enough cash on you"
     Comment "Barber refusal: the fee is more than the cash carried (#3234)."
    }
    CustomStringTableItem "{79887C4EA5818614}" {
     Id "LF-Barber_RefuseNoCharacter"
     Target_en_us "The barber cannot see you"
     Comment "Barber refusal: no character or record key (#3234)."
    }
    CustomStringTableItem "{B5EA72B91B44A3F8}" {
     Id "LF-Barber_RefuseNoLook"
     Target_en_us "The barber does not know that look"
     Comment "Barber refusal: look index out of range (#3234)."
    }
    CustomStringTableItem "{7E370923ED0BF27F}" {
     Id "LF-Barber_RefuseNoSeat"
     Target_en_us "Sit in the barber chair first"
     Comment "Barber refusal: the player is not seated on a barber chair (#3234)."
    }
    CustomStringTableItem "{F7263FF14E397E25}" {
     Id "LF-Barber_RefuseOff"
     Target_en_us "The barber is closed"
     Comment "Barber refusal: the feature is off (#3234)."
    }
    CustomStringTableItem "{CCD3BB259D0AFB7E}" {
     Id "LF-Barber_RefuseSameLook"
     Target_en_us "That is already your look"
     Comment "Barber refusal: the chosen look is the one worn (#3234)."
    }
    CustomStringTableItem "{E6E5C23EA2F7B092}" {
     Id "LF-Barber_Title"
     Target_en_us "Barber"
     Comment "Title of the barber sheet (#3234)."
    }
    CustomStringTableItem "{30B35DD36DE9AD2B}" {
     Id "LF-Bed_Action"
     Target_en_us "Put patient in bed"
     Comment "Bed action name."
    }
    CustomStringTableItem "{3A3A0C3CAF925A84}" {
     Id "LF-Bed_Desc"
     Target_en_us "Lay the downed patient you carry, or the one in the ambulance, in this bed"
     Comment "Bed action description."
    }
    CustomStringTableItem "{E72E14B2B5780800}" {
     Id "LF-Bed_Done"
     Target_en_us "The patient is in bed and awake."
     Comment "Toast to the medic after a delivery."
    }
    CustomStringTableItem "{F9207C344391D50C}" {
     Id "LF-Bed_NoPatient"
     Target_en_us "No downed patient to put in bed: carry one here or load them in the ambulance outside."
     Comment "Refusal: nobody to deliver."
    }
    CustomStringTableItem "{3055EF1F232FA28A}" {
     Id "LF-Bed_NotMedic"
     Target_en_us "Only an on-duty {medic} can admit a patient."
     Comment "Refusal: not a medic on duty."
    }
    CustomStringTableItem "{A22291637AB2A76D}" {
     Id "LF-Bed_TooFar"
     Target_en_us "Stand at the bed to put a patient in it."
     Comment "Refusal: medic too far from the bed."
    }
    CustomStringTableItem "{EE2695450DA92ACF}" {
     Id "LF-BigJob_Busy"
     Target_en_us "Another big job is already underway on the island. Wait until it is over."
     Comment "Big-job lock refusal (Feature/Crime/LF_BigJobLock.c): one pharmacy, bank or depot job runs on the server at a time."
    }
    CustomStringTableItem "{F9776BD9715991D9}" {
     Id "LF-BigJob_Cooldown"
     Target_en_us "This place was hit recently. Try again in %1 min."
     Comment "Big-job lock refusal (Feature/Crime/LF_BigJobLock.c): the job is cooling down. %1 is the minutes left."
    }
    CustomStringTableItem "{BE941138C8540332}" {
     Id "LF-BigJob_Unavailable"
     Target_en_us "This job cannot be started right now."
     Comment "Big-job lock refusal (Feature/Crime/LF_BigJobLock.c): a broken request (no job or player key)."
    }
    CustomStringTableItem "{CEF674540C6B082E}" {
     Id "LF-Bill_NeedCash"
     Target_en_us "Carry %1 in cash to pay the bill"
     Comment "Reason the desk payment is greyed out. %1 is the amount."
    }
    CustomStringTableItem "{60B397ACEEF7B02C}" {
     Id "LF-Bill_NoneOpen"
     Target_en_us "You carry no unpaid treatment bill."
     Comment "Toast at the hospital desk with no unpaid bill."
    }
    CustomStringTableItem "{0BD2574DC8D97B52}" {
     Id "LF-Bill_NotEnoughCash"
     Target_en_us "You need %1 in cash to pay the bill."
     Comment "Toast when the payer carries too little cash. %1 is the amount."
    }
    CustomStringTableItem "{0DF0D2453D53E9A5}" {
     Id "LF-Bill_PaidToast"
     Target_en_us "Bill paid: %1."
     Comment "Toast after paying a treatment bill. %1 is the amount."
    }
    CustomStringTableItem "{AA5D9FF8F7009C02}" {
     Id "LF-Bill_Pay"
     Target_en_us "Pay bill No. %1 (%2)"
     Comment "Desk payment action. %1 is the bill number, %2 the amount."
    }
    CustomStringTableItem "{A44CFB3B29598A73}" {
     Id "LF-Bill_PayDesc"
     Target_en_us "Pay a treatment bill in carried cash. The desk stamps it paid."
     Comment "Description of the hospital desk payment action."
    }
    CustomStringTableItem "{BADA3A253C27B54A}" {
     Id "LF-Bill_PayGeneric"
     Target_en_us "Pay bill"
     Comment "Fallback name of the hospital desk payment action."
    }
    CustomStringTableItem "{79B69A24A6715B00}" {
     Id "LF-Bill_Received"
     Target_en_us "The {medic} left you a treatment bill of %1. Pay it in cash at the hospital desk."
     Comment "Toast to a revived patient. %1 is the amount."
    }
    CustomStringTableItem "{20DCFFDAB028772B}" {
     Id "LF-Bill_ShareToast"
     Target_en_us "Your share of paid treatment bills: %1."
     Comment "Toast to a medic at clock-out. %1 is the cash paid."
    }
    CustomStringTableItem "{1B8B9B22A8964F7B}" {
     Id "LF-Bill_Title"
     Target_en_us "Treatment bill"
     Comment "Notification title for treatment bills."
    }
    CustomStringTableItem "{3C8E1F5A7D2B9406}" {
     Id "LF-Bin_Name"
     Target_en_us "Trash can"
     Comment "Inventory name of a tagged bin: an open container that is emptied by the world reset."
    }
    CustomStringTableItem "{B7A20D6E94C31F58}" {
     Id "LF-Bin_Open"
     Target_en_us "Look in trash can"
     Comment "Action on a bin: opens it as normal storage."
    }
    CustomStringTableItem "{293BEFA5FB5FC2B2}" {
     Id "LF-BookingDesk_Book"
     Target_en_us "Book suspect"
     Comment "Officer action on the booking desk: book the cuffed suspect beside them."
    }
    CustomStringTableItem "{8878A3308543871F}" {
     Id "LF-BookingDesk_BookDesc"
     Target_en_us "Charge the cuffed suspect beside you from their open crime reports."
     Comment "Description of the booking desk action."
    }
    CustomStringTableItem "{E594E2FD93031AF6}" {
     Id "LF-BookingDesk_NoCharges"
     Target_en_us "This suspect has no open crime reports to charge."
     Comment "Refusal toast: nothing to charge."
    }
    CustomStringTableItem "{AB63EECA4E69E45E}" {
     Id "LF-BookingDesk_NoOffer"
     Target_en_us "You have no open booking offer."
     Comment "Refusal toast to a suspect at the desk."
    }
    CustomStringTableItem "{451E083737C61271}" {
     Id "LF-BookingDesk_NoSuspect"
     Target_en_us "Bring the cuffed suspect beside you to book them."
     Comment "Refusal toast: no cuffed suspect next to the officer."
    }
    CustomStringTableItem "{CCE5E7A7F854490C}" {
     Id "LF-BookingDesk_NotHere"
     Target_en_us "Stand at the booking desk to book a suspect."
     Comment "Refusal toast: the officer is too far from the desk."
    }
    CustomStringTableItem "{F52CFB9B10946A42}" {
     Id "LF-BookingDesk_NotOnDuty"
     Target_en_us "Only an on-duty officer can book a suspect."
     Comment "Refusal toast at the booking desk."
    }
    CustomStringTableItem "{B4D1D0BA2B144D2B}" {
     Id "LF-BookingDesk_OfferOpen"
     Target_en_us "This suspect already has an open offer. Wait for their answer."
     Comment "Refusal toast: a second booking while an offer is open."
    }
    CustomStringTableItem "{4F8886A116F29389}" {
     Id "LF-BookingDesk_OfferSuspect"
     Target_en_us "You are booked: %1. Use the booking desk to pay now or take the time."
     Comment "Toast to the suspect. %1 = the terms, e.g. '$450 or 10 min'."
    }
    CustomStringTableItem "{C0EEBA23914CB0C0}" {
     Id "LF-BookingDesk_Offered"
     Target_en_us "Suspect booked: %1."
     Comment "Toast to the officer. %1 = the terms, e.g. '$450 or 10 min'."
    }
    CustomStringTableItem "{B5946EAD645CA27D}" {
     Id "LF-BookingDesk_OfficerGone"
     Target_en_us "The booking officer has left. Ask an officer to book you again."
     Comment "Toast: the officer who made the offer is gone."
    }
    CustomStringTableItem "{DFCCB05F185F73C7}" {
     Id "LF-BookingDesk_Pay"
     Target_en_us "Pay {currency:sym}%1 now"
     Comment "Suspect action on the booking desk. %1 = the fine in dollars."
    }
    CustomStringTableItem "{F1D9BD9D6D6D5724}" {
     Id "LF-BookingDesk_PayClosed"
     Target_en_us "Paying at the desk is not open yet. Take the time instead."
     Comment "Toast: the pay-now payment is not wired yet."
    }
    CustomStringTableItem "{8FEBD62161726822}" {
     Id "LF-BookingDesk_PayName"
     Target_en_us "Pay the fine now"
     Comment "Fallback name of the pay-now desk action."
    }
    CustomStringTableItem "{F2D64E8553CB15FA}" {
     Id "LF-BookingDesk_Time"
     Target_en_us "Take the time (%1 min)"
     Comment "Suspect action on the booking desk. %1 = minutes in the cell."
    }
    CustomStringTableItem "{CCB82C75C103A361}" {
     Id "LF-BookingDesk_TimeName"
     Target_en_us "Take the time"
     Comment "Fallback name of the take-the-time desk action."
    }
    CustomStringTableItem "{79A50A782818ECCC}" {
     Id "LF-BookingDesk_Title"
     Target_en_us "Booking desk"
     Comment "Toast title for the police booking desk."
    }
    CustomStringTableItem "{312172B24DB5B820}" {
     Id "LF-BreakIn_Barely"
     Target_en_us "Barely started"
     Comment "Break-in progress word: under a third of the way."
    }
    CustomStringTableItem "{C4526971F8C1E519}" {
     Id "LF-BreakIn_Halfway"
     Target_en_us "Halfway"
     Comment "Break-in progress word: about halfway."
    }
    CustomStringTableItem "{77D4B7DDEE0EFF43}" {
     Id "LF-BreakIn_Nearly"
     Target_en_us "Nearly through"
     Comment "Break-in progress word: close to done."
    }
    CustomStringTableItem "{54827F9A7CED2A2C}" {
     Id "LF-BreakIn_Through"
     Target_en_us "Through"
     Comment "Break-in progress word: the last stage is finished."
    }
    CustomStringTableItem "{F2A63B8D5C07E914}" {
     Id "LF-BuildingLock_Closed"
     Target_en_us "Closed"
     Comment "Door prompt on a building a lock marker shuts: closed"
    }
    CustomStringTableItem "{B94F2C60E8A31D75}" {
     Id "LF-BuildingLock_ForSaleSoon"
     Target_en_us "Closed: for sale soon"
     Comment "Door prompt on a building a lock marker shuts: for sale soon"
    }
    CustomStringTableItem "{0D58E7A3C1B6924F}" {
     Id "LF-BuildingLock_OpeningSoon"
     Target_en_us "Closed: opening soon"
     Comment "Door prompt on a building a lock marker shuts: opening soon"
    }
    CustomStringTableItem "{3A7C91E05B2D4F18}" {
     Id "LF-BuildingLock_Private"
     Target_en_us "Private property"
     Comment "Door prompt on a building a lock marker shuts: private"
    }
    CustomStringTableItem "{6E1B84D2A0C95F37}" {
     Id "LF-BuildingLock_StaffOnly"
     Target_en_us "Staff only"
     Comment "Door prompt on a building a lock marker shuts: staff only"
    }
    CustomStringTableItem "{950C69C056C83985}" {
     Id "LF-BusStop_Action"
     Target_en_us "Take the bus"
     Comment "Bus stop action fallback name (a slot with no stop is hidden)."
    }
    CustomStringTableItem "{B29A8D0906A937C3}" {
     Id "LF-BusStop_ActionTo"
     Target_en_us "Bus to %1: {currency:sym}%2, %3 s"
     Comment "Bus stop action, one per destination. %1 the stop name, %2 the fare in cash, %3 seconds of wait."
    }
    CustomStringTableItem "{FBFD6EFAC0C8BAE5}" {
     Id "LF-BusStop_Arrived"
     Target_en_us "You get off at %1."
     Comment "Bus toast on arrival. %1 the stop's name."
    }
    CustomStringTableItem "{8205388EAC928159}" {
     Id "LF-BusStop_BoardFare"
     Target_en_us "{currency:sym}%1"
     Comment "Timetable board: the fare to one stop. %1 the fare in cash."
    }
    CustomStringTableItem "{7D1971DA5E0F73C4}" {
     Id "LF-BusStop_BoardHere"
     Target_en_us "You are here"
     Comment "Timetable board: the mark on the reader's own stop."
    }
    CustomStringTableItem "{C60B8ECDA5501469}" {
     Id "LF-BusStop_BoardNext"
     Target_en_us "Pick your stop in the sign's action list, pay the fare in cash and wait at the stop."
     Comment "Timetable sign card: what to do next."
    }
    CustomStringTableItem "{60F8C241BEAFD60D}" {
     Id "LF-BusStop_BoardTitle"
     Target_en_us "Bus timetable"
     Comment "Printed heading of the timetable board on a bus stop pole. A pack names its line."
    }
    CustomStringTableItem "{37586E8F50271ECB}" {
     Id "LF-BusStop_BoardWhat"
     Target_en_us "Every stop on this line and the cash fare to it from here."
     Comment "Timetable sign card: what the board shows."
    }
    CustomStringTableItem "{877447BCF40EC4B5}" {
     Id "LF-BusStop_Boarded"
     Target_en_us "Ticket paid ({currency:sym}%1). The bus comes in %2 s: wait at the stop."
     Comment "Bus ticket bought. %1 the fare, %2 seconds until it leaves."
    }
    CustomStringTableItem "{5AA92EECCAE01BD7}" {
     Id "LF-BusStop_Busy"
     Target_en_us "You already have a ticket. Wait for the bus."
     Comment "Bus refusal: a trip is already waiting."
    }
    CustomStringTableItem "{D451B9D7F357DBB4}" {
     Id "LF-BusStop_CancelAction"
     Target_en_us "Cancel trip (refund {currency:sym}%1): bus in %2 s"
     Comment "Bus stop action while a ticket waits. %1 the fare refunded, %2 seconds until the bus leaves."
    }
    CustomStringTableItem "{4414BA244BADCE0C}" {
     Id "LF-BusStop_CancelAsked"
     Target_en_us "Trip canceled. Fare refunded."
     Comment "Bus toast: the player cancelled the waiting ticket."
    }
    CustomStringTableItem "{157F9BBFC17C40C2}" {
     Id "LF-BusStop_CancelHurt"
     Target_en_us "Trouble at the stop. Fare refunded."
     Comment "Bus toast: the wait was cancelled by damage or a fight."
    }
    CustomStringTableItem "{F384123F6B414805}" {
     Id "LF-BusStop_CancelMoved"
     Target_en_us "You left the stop. Fare refunded."
     Comment "Bus toast: the wait was cancelled because the player walked away."
    }
    CustomStringTableItem "{A75E225C4DBB2344}" {
     Id "LF-BusStop_CancelNoArrival"
     Target_en_us "The far stop is blocked. Fare refunded."
     Comment "Bus toast: no clear ground at the destination stop."
    }
    CustomStringTableItem "{C9F34037FCA484C5}" {
     Id "LF-BusStop_CancelState"
     Target_en_us "The bus could not take you. Fare refunded."
     Comment "Bus toast: the wait was cancelled (downed, cuffed, in a vehicle or gone)."
    }
    CustomStringTableItem "{BB22261A6CC4006C}" {
     Id "LF-BusStop_CancelTitle"
     Target_en_us "Cancel trip"
     Comment "Bus stop action name in the prefab (the script fills in the fare and the countdown)."
    }
    CustomStringTableItem "{4B1037495C5FD284}" {
     Id "LF-BusStop_Combat"
     Target_en_us "Not in the middle of a fight."
     Comment "Bus refusal: player-on-player damage just now."
    }
    CustomStringTableItem "{90CF3A002F678C51}" {
     Id "LF-BusStop_Cuffed"
     Target_en_us "Not while restrained."
     Comment "Bus refusal: cuffed, escorted or jailed."
    }
    CustomStringTableItem "{85574F3C6DA7EC77}" {
     Id "LF-BusStop_Dead"
     Target_en_us "You cannot travel like this."
     Comment "Bus refusal: the character is dead."
    }
    CustomStringTableItem "{6579493E43551662}" {
     Id "LF-BusStop_Downed"
     Target_en_us "You are hurt too badly to travel."
     Comment "Bus refusal: the character is downed."
    }
    CustomStringTableItem "{066B1C6D5DABA0CC}" {
     Id "LF-BusStop_Driving"
     Target_en_us "Get out of the vehicle first."
     Comment "Bus refusal: the player is in a vehicle."
    }
    CustomStringTableItem "{34BE773FFD417AE1}" {
     Id "LF-BusStop_GridNorth"
     Target_en_us "Test yard, north stop"
     Comment "Debug grid bus stop name."
    }
    CustomStringTableItem "{3A30D291DD7F9391}" {
     Id "LF-BusStop_GridSouth"
     Target_en_us "Test yard, south stop"
     Comment "Debug grid bus stop name."
    }
    CustomStringTableItem "{917B6AE39DA8392B}" {
     Id "LF-BusStop_NoCash"
     Target_en_us "You do not have the fare in cash."
     Comment "Bus refusal: not enough cash on the character."
    }
    CustomStringTableItem "{94226B8949AC68DF}" {
     Id "LF-BusStop_NoCharacter"
     Target_en_us "You cannot travel right now."
     Comment "Bus refusal: no character or no body."
    }
    CustomStringTableItem "{E831F4748B626430}" {
     Id "LF-BusStop_NoStop"
     Target_en_us "That stop is not in service."
     Comment "Bus refusal: a stop id does not resolve."
    }
    CustomStringTableItem "{ECEE3370AC404CF1}" {
     Id "LF-BusStop_NotAtStop"
     Target_en_us "Stand at the stop to buy a ticket."
     Comment "Bus refusal: the player is too far from the sign."
    }
    CustomStringTableItem "{51E5F33D3681F447}" {
     Id "LF-BusStop_Pursuit"
     Target_en_us "The driver will not stop with {police} on your tail."
     Comment "Bus refusal: wanted with an on-duty officer close."
    }
    CustomStringTableItem "{BCB996F6837C3299}" {
     Id "LF-BusStop_SameStop"
     Target_en_us "You are already at that stop."
     Comment "Bus refusal: the destination is this stop."
    }
    CustomStringTableItem "{333EA45031FB3696}" {
     Id "LF-BusStop_Title"
     Target_en_us "BUS STOP"
     Comment "Printed heading of the bus timetable and the bus toasts."
    }
    CustomStringTableItem "{6F547920FFED03C6}" {
     Id "LF-BusStop_TooSoon"
     Target_en_us "The next bus is not due yet. Try again shortly."
     Comment "Bus refusal: the request or trip cooldown."
    }
    CustomStringTableItem "{2192680E4E040B4E}" {
     Id "LF-BusStop_WhyBusy"
     Target_en_us "Ticket already bought"
     Comment "Greyed bus action reason: a ticket is already waiting."
    }
    CustomStringTableItem "{D3E1EB179C2F1379}" {
     Id "LF-BusStop_WhyNoCash"
     Target_en_us "Not enough cash"
     Comment "Greyed bus action reason: the fare is more than the cash carried."
    }
    CustomStringTableItem "{CC7F4F43A1B2B050}" {
     Id "LF-BusStop_WhyPursuit"
     Target_en_us "{police} nearby"
     Comment "Greyed bus action reason: wanted with an on-duty officer close."
    }
    CustomStringTableItem "{54D8814DF61E3B22}" {
     Id "LF-BusStop_WhyTooSoon"
     Target_en_us "Next bus not due yet"
     Comment "Greyed bus action reason: the ask or trip cooldown."
    }
    CustomStringTableItem "{7E1C40B2A93D5F06}" {
     Id "LF-BuyerSale_Action"
     Target_en_us "Sell the goods"
     Comment "Illicit buyer action: sell every chain good carried (#2920)."
    }
    CustomStringTableItem "{2A9F63D1C84B7E50}" {
     Id "LF-BuyerSale_ActionDesc"
     Target_en_us "He pays for what he names."
     Comment "Illicit buyer action description (#2920)."
    }
    CustomStringTableItem "{B05D8E27F1963A4C}" {
     Id "LF-BuyerSale_Closed"
     Target_en_us "He is not buying here, not today."
     Comment "Toast: the buyer is absent, away from the open spot, or the price list is missing (#2920)."
    }
    CustomStringTableItem "{4C7A12E9D60B83F5}" {
     Id "LF-BuyerSale_Nothing"
     Target_en_us "You have nothing he wants."
     Comment "Toast and cannot-perform reason: no chain goods the buyer names (#2920)."
    }
    CustomStringTableItem "{D83B5A0614EF9C27}" {
     Id "LF-BuyerSale_Sold"
     Target_en_us "He takes the goods and pays you %1."
     Comment "Toast: goods sold to the illicit buyer. %1 = the amount paid (#2920)."
    }
    CustomStringTableItem "{19E6C4A7B25D0F83}" {
     Id "LF-BuyerSale_Title"
     Target_en_us "The buyer"
     Comment "Toast title at the illicit buyer (#2920)."
    }
    CustomStringTableItem "{7724C0B2311D85FD}" {
     Id "LF-CC_Error_NoIdentity"
     Target_en_us "The online service has not confirmed who you are yet, so your account cannot open. Sign the form again in a moment, or rejoin."
     Comment "Written on form R-89 when the server still has no platform identity for the player after waiting for it."
    }
    CustomStringTableItem "{EF99F8D0F3808C85}" {
     Id "LF-CC_Wait_Identity"
     Target_en_us "Connecting to the online service. Your registration form opens in a moment."
     Comment "Toast while the server waits for the joining player's platform identity before it opens their account (a listen host gets it seconds after the join)."
    }
    CustomStringTableItem "{1865CC4D25E5D9B7}" {
     Id "LF-CallAbuse_LowPriority"
     Target_en_us "Low priority: caller charged for false reports"
     Comment "Dispatch board tag on a call whose caller was charged for false reports today (#4421)."
    }
    CustomStringTableItem "{4FCE393F14E712BB}" {
     Id "LF-CallOutcome_ActionNoOneFound"
     Target_en_us "Radio: no one found"
     Comment "Police car radio row (#4585): close the call the officer holds; the suspect was gone."
    }
    CustomStringTableItem "{BCB191D62965EE1B}" {
     Id "LF-CallOutcome_ActionResolved"
     Target_en_us "Radio: call resolved"
     Comment "Police car radio row (#4585): close the call the officer holds as resolved (a fine, an arrest or a revive settled it)."
    }
    CustomStringTableItem "{4F3D79F1FCC93949}" {
     Id "LF-CallOutcome_ActionUnfounded"
     Target_en_us "Radio: call unfounded"
     Comment "Police car radio row (#4585): close the call the officer holds as unfounded (nothing happened; counts against the caller)."
    }
    CustomStringTableItem "{AEB31D9E1738B09F}" {
     Id "LF-CallOutcome_NoCall"
     Target_en_us "That call is not open."
     Comment "Toast to the officer: the call is closed, unknown or no outcome was chosen (#4203)."
    }
    CustomStringTableItem "{0FC408BF75CB2693}" {
     Id "LF-CallOutcome_NoOneFound"
     Target_en_us "Call closed: no one found."
     Comment "Toast to the officer: the call closed with the outcome no one found (#4203)."
    }
    CustomStringTableItem "{0B1CFA4FF5F9A337}" {
     Id "LF-CallOutcome_NoScene"
     Target_en_us "Dispatch cannot place this call on the map. Close it with a fine at the scene."
     Comment "Toast to the officer: the call has no position to check presence against (#4203)."
    }
    CustomStringTableItem "{02A2956C3E052118}" {
     Id "LF-CallOutcome_NoneHeld"
     Target_en_us "You hold no call. Take one at the dispatch board."
     Comment "Toast when an officer radios a call closed without holding one (#4585)."
    }
    CustomStringTableItem "{E371BE1A6F20F92F}" {
     Id "LF-CallOutcome_NotHeld"
     Target_en_us "That call is not yours."
     Comment "Toast to the officer: another officer holds the call (#4203)."
    }
    CustomStringTableItem "{B6C9B26C8B13D5D5}" {
     Id "LF-CallOutcome_Resolved"
     Target_en_us "Call closed: resolved."
     Comment "Toast to the officer: the call closed as resolved (#4203)."
    }
    CustomStringTableItem "{BF2441E69414044F}" {
     Id "LF-CallOutcome_TooFar"
     Target_en_us "You are too far from the scene to close this call. Get within %1 m."
     Comment "Toast to the officer: a close from outside the scene radius; %1 is the radius in meters (#4203)."
    }
    CustomStringTableItem "{020D9D70E1490659}" {
     Id "LF-CallOutcome_Unfounded"
     Target_en_us "Call closed: unfounded."
     Comment "Toast to the officer: the call closed as unfounded (#4203)."
    }
    CustomStringTableItem "{CCA19DCF527EF2A2}" {
     Id "LF-CarPool_BayBlocked"
     Target_en_us "The bay is blocked. Move the car out of the bay first."
     Comment "Toast (#4352): a vehicle stands in the car pool's bay."
    }
    CustomStringTableItem "{0A300420FA34288B}" {
     Id "LF-CarPool_CarOut"
     Target_en_us "Your last car is still out with people in it."
     Comment "Toast (#4352): an officer's previous pool car is occupied, so it cannot go back for a new one."
    }
    CustomStringTableItem "{0A273A404E690928}" {
     Id "LF-CarPool_Drawn"
     Target_en_us "Your car is in the bay."
     Comment "Toast (#4352): the car pool put the officer's car in the bay."
    }
    CustomStringTableItem "{793A240492C52CCA}" {
     Id "LF-CarPool_OffDuty"
     Target_en_us "Clock in before you take a car from the pool."
     Comment "Toast (#4352): the car pool refused a player who is not police on duty."
    }
    CustomStringTableItem "{D099389B5BAEADCD}" {
     Id "LF-CarPool_Take"
     Target_en_us "Take the %1 car"
     Comment "Action on the police car pool board (#4352). %1 = the title of the lowest rank that draws this car (Cadet, Patrol)."
    }
    CustomStringTableItem "{2552B4871B415278}" {
     Id "LF-CarPool_TooFar"
     Target_en_us "Stand at the car pool board to take a car."
     Comment "Toast (#4352): the officer is too far from the pool board."
    }
    CustomStringTableItem "{8EC773517A0DB2DD}" {
     Id "LF-CarPool_TooSoon"
     Target_en_us "You just took a car. Wait a moment."
     Comment "Toast (#4352): the officer drew a car seconds ago."
    }
    CustomStringTableItem "{B27102B0C486F20E}" {
     Id "LF-Cargo_Empty"
     Target_en_us "The bed is empty."
     Comment "Toast: unload pressed on an empty bed (#3265)."
    }
    CustomStringTableItem "{3B4A64303E9729FD}" {
     Id "LF-Cargo_Full"
     Target_en_us "The bed is full."
     Comment "Toast and reason: no room for this load (#3265)."
    }
    CustomStringTableItem "{EE82A1CD38C1F4B8}" {
     Id "LF-Cargo_HandsFull"
     Target_en_us "Your hands are full."
     Comment "Toast and reason: unload needs free hands (#3265)."
    }
    CustomStringTableItem "{26FEFFF494632DD3}" {
     Id "LF-Cargo_Load"
     Target_en_us "Load onto the truck"
     Comment "F-key action name: put the carried load on the bed (#3265)."
    }
    CustomStringTableItem "{0854C8AB56AD9EFB}" {
     Id "LF-Cargo_NoBed"
     Target_en_us "This vehicle has no cargo bed."
     Comment "Toast: a car cannot carry a load (#3265)."
    }
    CustomStringTableItem "{07D0C122499A5FB9}" {
     Id "LF-Cargo_NoItemPrefab"
     Target_en_us "That load can't come off the truck yet."
     Comment "Toast: the top load of the bed has no item to put on the shoulder (#4886)."
    }
    CustomStringTableItem "{BF7D47302660AF18}" {
     Id "LF-Cargo_NoLoad"
     Target_en_us "You are not carrying anything."
     Comment "Toast: load pressed with empty shoulders (#3265)."
    }
    CustomStringTableItem "{F17DC59B6A1E5566}" {
     Id "LF-Cargo_NotAccepted"
     Target_en_us "The bed does not take this."
     Comment "Toast: a kind the bed refuses (#3265)."
    }
    CustomStringTableItem "{1DCA2E1743D81081}" {
     Id "LF-Cargo_Title"
     Target_en_us "Cargo"
     Comment "Toast title for the truck bed (#3265)."
    }
    CustomStringTableItem "{41666EAF598EC0C9}" {
     Id "LF-Cargo_TooFar"
     Target_en_us "Move closer to the tailgate."
     Comment "Toast and reason: the carrier stands too far from the bed to load or unload (#4886)."
    }
    CustomStringTableItem "{B742E763B9E68070}" {
     Id "LF-Cargo_Unload"
     Target_en_us "Unload onto your shoulder"
     Comment "F-key action name: take the top load off the bed (#3265)."
    }
    CustomStringTableItem "{7A30C5E1D49B6A08}" {
     Id "LF-Carry_Busy"
     Target_en_us "Your hands are full."
     Comment "Carry refusal: the carrier already holds a body or is in a vehicle."
    }
    CustomStringTableItem "{7A30C5E1D49B6A02}" {
     Id "LF-Carry_Carry"
     Target_en_us "Carry"
     Comment "Player action: carry a downed player."
    }
    CustomStringTableItem "{7A30C5E1D49B6A01}" {
     Id "LF-Carry_Drag"
     Target_en_us "Drag"
     Comment "Player action: drag a downed player."
    }
    CustomStringTableItem "{7A30C5E1D49B6A03}" {
     Id "LF-Carry_Drop"
     Target_en_us "Put down"
     Comment "Player action: the carrier puts the body down."
    }
    CustomStringTableItem "{7A30C5E1D49B6A07}" {
     Id "LF-Carry_Held"
     Target_en_us "Someone already has them."
     Comment "Carry refusal: the body is already held."
    }
    CustomStringTableItem "{7A30C5E1D49B6A04}" {
     Id "LF-Carry_Load"
     Target_en_us "Load into vehicle"
     Comment "Player action: put the carried player in a free passenger seat."
    }
    CustomStringTableItem "{7A30C5E1D49B6A09}" {
     Id "LF-Carry_NoSeat"
     Target_en_us "No free seat in reach."
     Comment "Load refusal: no vehicle with a free passenger seat nearby."
    }
    CustomStringTableItem "{7A30C5E1D49B6A05}" {
     Id "LF-Carry_NotAllowedDuty"
     Target_en_us "Only a {medic} or {police} on duty can move them."
     Comment "Carry refusal: config limits it to medics and police."
    }
    CustomStringTableItem "{7A30C5E1D49B6A0B}" {
     Id "LF-Carry_NotDowned"
     Target_en_us "They are not down."
     Comment "Carry refusal: the target is not downed."
    }
    CustomStringTableItem "{7A30C5E1D49B6A0A}" {
     Id "LF-Carry_NotHeld"
     Target_en_us "You are not holding anyone."
     Comment "Drop or load with nothing held."
    }
    CustomStringTableItem "{7A30C5E1D49B6A06}" {
     Id "LF-Carry_TooFar"
     Target_en_us "Too far away."
     Comment "Carry refusal: out of range."
    }
    CustomStringTableItem "{78EF82E35BB73ADD}" {
     Id "LF-CaseBoard_CaseHeader"
     Target_en_us "Case %1"
     Comment "Header of a case sheet on the board face; %1 is the report number. Printed in capitals (#148)."
    }
    CustomStringTableItem "{498C53D90C725BD5}" {
     Id "LF-CaseBoard_FootprintNote"
     Target_en_us "Print of %1"
     Comment "Footprint card line; %1 is the suspect's footwear (#148)."
    }
    CustomStringTableItem "{F5143D4581991E31}" {
     Id "LF-CaseBoard_Kind_casing"
     Target_en_us "Shell casing"
     Comment "Evidence card title (#148)."
    }
    CustomStringTableItem "{6A7F76059AF3CDD5}" {
     Id "LF-CaseBoard_Kind_dropped"
     Target_en_us "Dropped item"
     Comment "Evidence card title (#148)."
    }
    CustomStringTableItem "{CC4F6A7D13DA03D0}" {
     Id "LF-CaseBoard_Kind_footprint"
     Target_en_us "Footprint"
     Comment "Evidence card title (#148)."
    }
    CustomStringTableItem "{B9E3D8B120B88890}" {
     Id "LF-CaseBoard_Kind_statement"
     Target_en_us "Witness statement"
     Comment "Evidence card title (#148)."
    }
    CustomStringTableItem "{B601E7BA839D3417}" {
     Id "LF-CaseBoard_NoNote"
     Target_en_us "Found at the scene"
     Comment "Evidence card line when the piece says nothing more (#148)."
    }
    CustomStringTableItem "{EDC95AB6DCC1E098}" {
     Id "LF-CaseBoard_NothingPinned"
     Target_en_us "Nothing is pinned to the board."
     Comment "Toast when unpinning finds nothing (#148)."
    }
    CustomStringTableItem "{7B64365A819C2F84}" {
     Id "LF-CaseBoard_NothingToPin"
     Target_en_us "No evidence to pin. Bag it at the scene or log it at the locker first."
     Comment "Toast when pinning finds nothing (#148)."
    }
    CustomStringTableItem "{0B29DE154C616F79}" {
     Id "LF-CaseBoard_Pin_Action"
     Target_en_us "Pin evidence"
     Comment "F-key action on the case board: pin carried evidence bags, or the next logged card (#148)."
    }
    CustomStringTableItem "{88E53769C33A742A}" {
     Id "LF-CaseBoard_Pinned"
     Target_en_us "%1 card(s) pinned to the case board."
     Comment "Toast after pinning; %1 is the number of cards (#148)."
    }
    CustomStringTableItem "{676FF529BB3B6D09}" {
     Id "LF-CaseBoard_PlaceTime"
     Target_en_us "%1, %2"
     Comment "A case's place and the time of the crime on the board face (#148)."
    }
    CustomStringTableItem "{960E19EA6B5BB672}" {
     Id "LF-CaseBoard_Title"
     Target_en_us "Case board"
     Comment "Toast title for the police case board (#148)."
    }
    CustomStringTableItem "{576F32E08182B47E}" {
     Id "LF-CaseBoard_Unpin_Action"
     Target_en_us "Unpin evidence"
     Comment "F-key action on the case board: take down the card pinned last (#148)."
    }
    CustomStringTableItem "{E3F153BC2DA3B463}" {
     Id "LF-CaseBoard_Unpinned"
     Target_en_us "Card taken down. It stays logged in the locker."
     Comment "Toast after unpinning (#148)."
    }
    CustomStringTableItem "{DE6F4994046855A2}" {
     Id "LF-CasualtyTreat_Action"
     Target_en_us "Treat the patient"
     Comment "Hold action on a hurt resident from a medic call (casualty), offered to an on-duty medic."
    }
    CustomStringTableItem "{95C4237C07E41A13}" {
     Id "LF-CasualtyTreat_Done"
     Target_en_us "Patient treated. The call is closed."
     Comment "Toast to the medic after treating a casualty."
    }
    CustomStringTableItem "{1407ED04A160A420}" {
     Id "LF-Cell_Locked"
     Target_en_us "The cell is locked"
     Comment "Door action cannot-perform reason on a locked jail cell door (LF_LockComponent m_sLockedReason)."
    }
    CustomStringTableItem "{C4A100000000A004}" {
     Id "LF-Chain_Cider_Name"
     Target_en_us "Cider"
     Comment "Name of the neutral example chain in ResourceChains.conf (#416)."
    }
    CustomStringTableItem "{C4A100000000A013}" {
     Id "LF-Chain_Collect"
     Target_en_us "Take the product"
     Comment "Chain station action: take a finished batch out (#417)."
    }
    CustomStringTableItem "{C4A100000000A019}" {
     Id "LF-Chain_Failed"
     Target_en_us "That did not work"
     Comment "Chain station refused: the product could not be delivered (#417)."
    }
    CustomStringTableItem "{C4A100000000A012}" {
     Id "LF-Chain_Load"
     Target_en_us "Load the batch"
     Comment "Chain station action: put a batch in (#417)."
    }
    CustomStringTableItem "{C4A100000000A017}" {
     Id "LF-Chain_MissingInputs"
     Target_en_us "You do not have what this needs"
     Comment "Chain station refused: the worker lacks the inputs (#417)."
    }
    CustomStringTableItem "{C4A100000000A0B1}" {
     Id "LF-Chain_Moonshine_Name"
     Target_en_us "Samogon"
     Comment "Name of the moonshine chain in ResourceChains.conf (#420)."
    }
    CustomStringTableItem "{C4A100000000A003}" {
     Id "LF-Chain_NeedEquipment"
     Target_en_us "You need the right equipment for this work"
     Comment "Chain step refused: the worker does not carry the equipment the step names (#416)."
    }
    CustomStringTableItem "{C4A100000000A002}" {
     Id "LF-Chain_NeedLevel"
     Target_en_us "You are not experienced enough for this work"
     Comment "Chain step refused: the worker is below the level the step names (#416)."
    }
    CustomStringTableItem "{C4A100000000A001}" {
     Id "LF-Chain_NeedLicense"
     Target_en_us "You are not licensed for this work"
     Comment "Chain step refused: the worker lacks the licence the step names (#416)."
    }
    CustomStringTableItem "{F7EF14B5A3B557CE}" {
     Id "LF-Chain_NeedLicenseWhere"
     Target_en_us "You are not licensed for this work. %1"
     Comment "Chain step refused for a missing licence (#3150): %1 says where to get it and what it costs."
    }
    CustomStringTableItem "{C4A100000000A0C4}" {
     Id "LF-Chain_SmellLead"
     Target_en_us "Smell of brewing"
     Comment "Dispatch summary of the police lead a noticed chain smell raises (#418)."
    }
    CustomStringTableItem "{C4A100000000A0C2}" {
     Id "LF-Chain_Smell_Officer"
     Target_en_us "Something sour and sweet is cooking nearby. Somebody is running a still."
     Comment "Toast to an on-duty officer inside the smell range of a running chain site (#418)."
    }
    CustomStringTableItem "{C4A100000000A0C1}" {
     Id "LF-Chain_Smell_Title"
     Target_en_us "Smell"
     Comment "Toast title for the chain smell cue (#418)."
    }
    CustomStringTableItem "{C4A100000000A0C3}" {
     Id "LF-Chain_Smell_Witness"
     Target_en_us "You catch a sharp, sour smell in the air. Something is being brewed nearby."
     Comment "Toast to a player inside the smell range of a running chain site (#418)."
    }
    CustomStringTableItem "{C4A100000000A011}" {
     Id "LF-Chain_Title"
     Target_en_us "Production"
     Comment "Toast title for chain station refusals (#417)."
    }
    CustomStringTableItem "{C4A100000000A018}" {
     Id "LF-Chain_TooFar"
     Target_en_us "Stand closer"
     Comment "Chain station refused: the worker is out of reach (#417)."
    }
    CustomStringTableItem "{C4A100000000A014}" {
     Id "LF-Chain_Work_Fresh"
     Target_en_us "Work it"
     Comment "Chain station hold action while the batch has barely started (#417)."
    }
    CustomStringTableItem "{C4A100000000A016}" {
     Id "LF-Chain_Work_Late"
     Target_en_us "Finish it off"
     Comment "Chain station hold action while the batch is nearly done (#417)."
    }
    CustomStringTableItem "{C4A100000000A015}" {
     Id "LF-Chain_Work_Mid"
     Target_en_us "Keep working it"
     Comment "Chain station hold action while the batch is part way (#417)."
    }
    CustomStringTableItem "{8B8EEE733934A3E5}" {
     Id "LF-CharSel_Intro"
     Target_en_us "Choose the resident you arrive as today."
     Comment "Character pick instruction line. One button per character follows, each showing that character's name."
    }
    CustomStringTableItem "{502C5430AE70C609}" {
     Id "LF-CharSel_New"
     Target_en_us "Register a new resident"
     Comment "Character pick button that opens registration form R-89 for another character on the same account."
    }
    CustomStringTableItem "{87EB7613EC87FB35}" {
     Id "LF-CharSel_Refused"
     Target_en_us "You cannot change who you are right now."
     Comment "Notification when the server refuses a character pick: the slot does not exist, the player is alive in another body, or still on duty."
    }
    CustomStringTableItem "{0D72D045F1F4E803}" {
     Id "LF-CharSel_Title"
     Target_en_us "Civil Registry, Residents on File"
     Comment "Character pick title on arrival, shown when an account may hold more than one character. A 1989 registry sheet."
    }
    CustomStringTableItem "{EC96C639C81618E1}" {
     Id "LF-CharSwitch_ClerkNo"
     Target_en_us "I cannot change your papers right now."
     Comment "Town clerk answer when a character switch is refused; a toast says why."
    }
    CustomStringTableItem "{D428313AD233AA61}" {
     Id "LF-CharSwitch_ClerkYes"
     Target_en_us "Of course. Which of your files shall I pull?"
     Comment "Town clerk answer when a character switch is allowed; the switch sheet opens after the talk."
    }
    CustomStringTableItem "{9A4F3971C8398625}" {
     Id "LF-CharSwitch_Combat"
     Target_en_us "Not in the middle of a fight. Wait a couple of minutes."
     Comment "Toast: character switch refused shortly after hitting or being hit by another player."
    }
    CustomStringTableItem "{2A1C08B1ACC157AC}" {
     Id "LF-CharSwitch_Contraband"
     Target_en_us "Not with contraband on you or a run under way."
     Comment "Toast: character switch refused while carrying contraband or hauling a convoy run."
    }
    CustomStringTableItem "{2899AD20654A0D26}" {
     Id "LF-CharSwitch_Cuffed"
     Target_en_us "Not while you are cuffed or in custody."
     Comment "Toast: character switch refused while cuffed, held or escorted."
    }
    CustomStringTableItem "{B6110ED9E8A0575C}" {
     Id "LF-CharSwitch_Downed"
     Target_en_us "Not while you are down."
     Comment "Toast: character switch refused while downed."
    }
    CustomStringTableItem "{945D348B9613DEA5}" {
     Id "LF-CharSwitch_Intro"
     Target_en_us "Choose which of your residents goes on from here. The one you put away keeps their place, papers and pockets until you come back to them."
     Comment "Character switch sheet instruction line. One button per other character follows."
    }
    CustomStringTableItem "{C61776D5D8142E8B}" {
     Id "LF-CharSwitch_Jailed"
     Target_en_us "Not while you are serving a sentence."
     Comment "Toast: character switch refused while jailed."
    }
    CustomStringTableItem "{7C49AC7D4A60754B}" {
     Id "LF-CharSwitch_JobRun"
     Target_en_us "Finish or cancel your job run first."
     Comment "Toast: character switch refused during an open delivery job run."
    }
    CustomStringTableItem "{083048FE55B906B8}" {
     Id "LF-CharSwitch_Off"
     Target_en_us "Only one file per person is kept here."
     Comment "Toast: the server allows one character per account, so there is nothing to switch to."
    }
    CustomStringTableItem "{46083630BC792DED}" {
     Id "LF-CharSwitch_OnDuty"
     Target_en_us "Clock off duty first."
     Comment "Toast: character switch refused while on police or medic duty."
    }
    CustomStringTableItem "{A4AD76E302FDB567}" {
     Id "LF-CharSwitch_Playing"
     Target_en_us "On file now"
     Comment "Character switch sheet: status beside the character being played."
    }
    CustomStringTableItem "{C5632848E7273BA4}" {
     Id "LF-CharSwitch_Racing"
     Target_en_us "Not while you are entered in a race."
     Comment "Toast: character switch refused while in a race."
    }
    CustomStringTableItem "{C5993D99EE2058E4}" {
     Id "LF-CharSwitch_Stay"
     Target_en_us "Stay as you are"
     Comment "Character switch sheet: closes the sheet and keeps the current character."
    }
    CustomStringTableItem "{1566950A275C9033}" {
     Id "LF-CharSwitch_Title"
     Target_en_us "Civil Registry, Change of File"
     Comment "Title of the character switch sheet and of its refusal toasts (#331). A 1989 registry office."
    }
    CustomStringTableItem "{C333F7EA551D3122}" {
     Id "LF-CharSwitch_Vehicle"
     Target_en_us "Step out of the vehicle first."
     Comment "Toast: character switch refused while in a vehicle."
    }
    CustomStringTableItem "{1E2628AE7332248D}" {
     Id "LF-CharSwitch_Wanted"
     Target_en_us "Not while you are wanted. Settle your record first."
     Comment "Toast: character switch refused while wanted."
    }
    CustomStringTableItem "{D05A58380EB506C0}" {
     Id "LF-Char_Error_Full"
     Target_en_us "Every character slot on this account is taken."
     Comment "Character creation refused because the account already holds the configured maximum number of characters."
    }
    CustomStringTableItem "{2DF6913A7B6E562C}" {
     Id "LF-ChargePayment_Failed"
     Target_en_us "The payment did not go through. Try again or take the time."
     Comment "Toast to the suspect: the money could not be taken; nothing was charged."
    }
    CustomStringTableItem "{1B6EC4D1194BE38F}" {
     Id "LF-ChargePayment_Paid"
     Target_en_us "You paid your fine of {currency:sym}%1. You are free to go."
     Comment "Toast to the suspect: the fine was paid at the booking desk. %1 = the fine in dollars."
    }
    CustomStringTableItem "{5142F0A862D4F263}" {
     Id "LF-ChargePayment_Short"
     Target_en_us "You can't cover {currency:sym}%1, so you take the time."
     Comment "Toast to the suspect: cash and bank fall short of the fine; they go to the cell. %1 = the fine in dollars."
    }
    CustomStringTableItem "{C7F8DEA56AF6FF3E}" {
     Id "LF-ChargeSheet_Capped"
     Target_en_us "Your rank limits this fine to {currency:sym}%1."
     Comment "Note under the total when the rank cap cut the fine. %1 = the cap in dollars."
    }
    CustomStringTableItem "{B8446036CAFF4715}" {
     Id "LF-ChargeSheet_Close"
     Target_en_us "Close"
     Comment "Button: put the charge sheet away unsigned."
    }
    CustomStringTableItem "{69E3DA5CDB56BD12}" {
     Id "LF-ChargeSheet_Minutes"
     Target_en_us "%1 min"
     Comment "Time on the charge sheet. %1 = minutes."
    }
    CustomStringTableItem "{C508E98273CBBFC9}" {
     Id "LF-ChargeSheet_Money"
     Target_en_us "{currency:sym}%1"
     Comment "Money amount on the charge sheet. %1 = dollars."
    }
    CustomStringTableItem "{66067AFF1E8DAEE8}" {
     Id "LF-ChargeSheet_NoSheet"
     Target_en_us "There is no open charge sheet. Book the suspect again."
     Comment "Toast: the signature came with no open sheet."
    }
    CustomStringTableItem "{D845C156A3BF5987}" {
     Id "LF-ChargeSheet_None"
     Target_en_us "None"
     Comment "Foot of the charge sheet: no fine or no time."
    }
    CustomStringTableItem "{C9A056B424C32FB9}" {
     Id "LF-ChargeSheet_NoneTicked"
     Target_en_us "Check at least one charge before you sign."
     Comment "Toast and note: nothing ticked on the sheet."
    }
    CustomStringTableItem "{835F52B991AB4548}" {
     Id "LF-ChargeSheet_NothingYet"
     Target_en_us "Nothing is charged until you sign."
     Comment "Note under the total before anything is ticked."
    }
    CustomStringTableItem "{F15EDBFA8B381B38}" {
     Id "LF-ChargeSheet_OverCap"
     Target_en_us "%1, over your limit"
     Comment "Greyed row status: the charge's own fine, which the rank fine cap cuts from the total. %1 = the charge fine as money text (LF-ChargeSheet_Money). Keep it short: the status column of the row is narrow and clips past about 25 characters (#5081); the note under the total explains the rank cap."
    }
    CustomStringTableItem "{36EB17C89AD9A18C}" {
     Id "LF-ChargeSheet_Pick"
     Target_en_us "Check the charges for this booking, then sign."
     Comment "Line under the charge sheet heading."
    }
    CustomStringTableItem "{E82CC390D85B24C1}" {
     Id "LF-ChargeSheet_ReadyNote"
     Target_en_us "Sign to offer the suspect the fine or the time."
     Comment "Note under the total once a charge is ticked."
    }
    CustomStringTableItem "{77A83752D2D25124}" {
     Id "LF-ChargeSheet_RowBoth"
     Target_en_us "{currency:sym}%1, %2 min"
     Comment "Row status: the charge fine and time. %1 = dollars, %2 = minutes."
    }
    CustomStringTableItem "{1C7B8A037C80487D}" {
     Id "LF-ChargeSheet_RowFine"
     Target_en_us "{currency:sym}%1"
     Comment "Row status: a fine-only charge. %1 = dollars."
    }
    CustomStringTableItem "{BE3A5D08F999C233}" {
     Id "LF-ChargeSheet_RowJail"
     Target_en_us "Jail, %1 min"
     Comment "Row status: a jail-only charge. %1 = minutes."
    }
    CustomStringTableItem "{70A741E7B4D11E90}" {
     Id "LF-ChargeSheet_Sign"
     Target_en_us "Sign"
     Comment "Button: sign the charge sheet and send the booking offer."
    }
    CustomStringTableItem "{C4B5F5A93D276780}" {
     Id "LF-ChargeSheet_Stacked"
     Target_en_us "Stacked"
     Comment "Greyed row status: another ticked charge of the same group counts instead."
    }
    CustomStringTableItem "{0891C336368D2A53}" {
     Id "LF-ChargeSheet_Tick"
     Target_en_us "Check"
     Comment "Button on an open charge row: add it to the sheet."
    }
    CustomStringTableItem "{946CEC656180D415}" {
     Id "LF-ChargeSheet_Title"
     Target_en_us "Charge sheet"
     Comment "Heading of the charge sheet paper the booking officer signs."
    }
    CustomStringTableItem "{6A50B06BBA8655E8}" {
     Id "LF-ChargeSheet_TotalFine"
     Target_en_us "Total fine"
     Comment "Foot of the charge sheet: the fine the sheet asks for."
    }
    CustomStringTableItem "{E3534A731156B2B9}" {
     Id "LF-ChargeSheet_TotalTime"
     Target_en_us "Time in the cell"
     Comment "Foot of the charge sheet: the time the sheet asks for."
    }
    CustomStringTableItem "{45572A2EE371E594}" {
     Id "LF-ChargeSheet_Untick"
     Target_en_us "Uncheck"
     Comment "Button on a ticked charge row: take it off the sheet."
    }
    CustomStringTableItem "{294B1C333502C014}" {
     Id "LF-Chess_BlackInCheck"
     Target_en_us "Black to move, in check"
     Comment "Chess board: whose turn, in check."
    }
    CustomStringTableItem "{DF5A83BF2E8A0BF7}" {
     Id "LF-Chess_BlackToMove"
     Target_en_us "Black to move"
     Comment "Chess board: whose turn."
    }
    CustomStringTableItem "{5ED12C260FF1021C}" {
     Id "LF-Chess_BlackWins"
     Target_en_us "Black wins"
     Comment "Chess board: the game ended."
    }
    CustomStringTableItem "{1F68D2A4E70B93C5}" {
     Id "LF-Chess_CannotAfford"
     Target_en_us "Not enough cash for the stake"
     Comment "Reason a staked chess action cannot be used: the player carries less than the stake."
    }
    CustomStringTableItem "{8441B5995A901878}" {
     Id "LF-Chess_Draw"
     Target_en_us "Drawn game"
     Comment "Chess board: the game ended in a draw."
    }
    CustomStringTableItem "{B43B12CB3A87A5CA}" {
     Id "LF-Chess_LastMove"
     Target_en_us "Last move: %1"
     Comment "Chess board: the last move played, such as e2e4."
    }
    CustomStringTableItem "{0903BC959C769DD9}" {
     Id "LF-Chess_Play"
     Target_en_us "Play chess"
     Comment "Action on a chess table: sit and play, or watch when both seats are taken."
    }
    CustomStringTableItem "{4C0E9B7A21D358F6}" {
     Id "LF-Chess_PlayFor"
     Target_en_us "Play for {currency:sym}%1"
     Comment "Action on a chess table: sit and play a game where each player puts the stake (%1, 20 or 50) in escrow and the winner takes the pot."
    }
    CustomStringTableItem "{EAB56E5EA3549E64}" {
     Id "LF-Chess_Selected"
     Target_en_us "Moving from %1: click where it goes"
     Comment "Chess board: a piece is picked up. %1 is a square such as e2."
    }
    CustomStringTableItem "{E7ED7DA4025B0B70}" {
     Id "LF-Chess_StandUp"
     Target_en_us "Stand up"
     Comment "Close button of the chess board; standing up mid-game resigns after a grace period."
    }
    CustomStringTableItem "{6484B0F23B8FE931}" {
     Id "LF-Chess_Title"
     Target_en_us "Chess"
     Comment "Title of the chess board sheet."
    }
    CustomStringTableItem "{475A1556AF20C80E}" {
     Id "LF-Chess_Waiting"
     Target_en_us "Waiting for an opponent to sit down"
     Comment "Chess board: one seat is empty."
    }
    CustomStringTableItem "{10A7EDA4F1EAEA27}" {
     Id "LF-Chess_Watching"
     Target_en_us "Both seats are taken; you are watching"
     Comment "Chess board: the reader holds no seat."
    }
    CustomStringTableItem "{16D1027DC7BC0180}" {
     Id "LF-Chess_WhiteInCheck"
     Target_en_us "White to move, in check"
     Comment "Chess board: whose turn, in check."
    }
    CustomStringTableItem "{7E0B60D70576A7CA}" {
     Id "LF-Chess_WhiteToMove"
     Target_en_us "White to move"
     Comment "Chess board: whose turn."
    }
    CustomStringTableItem "{541A8E2D2D07B46F}" {
     Id "LF-Chess_WhiteWins"
     Target_en_us "White wins"
     Comment "Chess board: the game ended."
    }
    CustomStringTableItem "{BC3DF558CF296BE6}" {
     Id "LF-Chess_YouBlack"
     Target_en_us "You play black"
     Comment "Chess board: your seat."
    }
    CustomStringTableItem "{5564712FA572FA16}" {
     Id "LF-Chess_YouWhite"
     Target_en_us "You play white"
     Comment "Chess board: your seat."
    }
    CustomStringTableItem "{53C4F6BAC7AAF841}" {
     Id "LF-ChopFit_Action"
     Target_en_us "Fit a hidden compartment"
     Comment "Action: fit a hidden compartment to your car at the chop shop bench (#3776)."
    }
    CustomStringTableItem "{067E82FC427CEEAC}" {
     Id "LF-ChopFit_AlreadyFitted"
     Target_en_us "This car already has a compartment"
     Comment "Toast: the car in the yard already has one (#3776)."
    }
    CustomStringTableItem "{1AD684CDD3AA9AF8}" {
     Id "LF-ChopFit_Done"
     Target_en_us "Compartment fitted for %1. Only you can open it."
     Comment "Toast: fitted; %1 = the price paid (#3776)."
    }
    CustomStringTableItem "{733264AC2EA28EAA}" {
     Id "LF-ChopFit_Failed"
     Target_en_us "The yard could not fit it. You were not charged"
     Comment "Toast: the compartment did not build and the price was handed back (#3776)."
    }
    CustomStringTableItem "{A8C83E644C7756E7}" {
     Id "LF-ChopFit_NoVehicle"
     Target_en_us "Park your car by the bench first"
     Comment "Toast: no car in the drop area (#3776)."
    }
    CustomStringTableItem "{26A5C4DD24616668}" {
     Id "LF-ChopFit_NotOwner"
     Target_en_us "That is not your car"
     Comment "Toast: the car in the yard is not owned by you (#3776)."
    }
    CustomStringTableItem "{1E8F5D0E98750F73}" {
     Id "LF-ChopFit_ShortPurse"
     Target_en_us "Fitting costs %1. You do not carry enough cash"
     Comment "Toast: short of cash; %1 = the price (#3776)."
    }
    CustomStringTableItem "{A337DA90CA34D072}" {
     Id "LF-ChopFit_TooFar"
     Target_en_us "Step up to the bench"
     Comment "Toast: too far from the bench (#3776)."
    }
    CustomStringTableItem "{8D7A5FEA2D016218}" {
     Id "LF-ChopFit_Unsupported"
     Target_en_us "The yard cannot fit a compartment in this car"
     Comment "Toast: the car has no row in the compartment table (#3776)."
    }
    CustomStringTableItem "{87229FC4BD0FB108}" {
     Id "LF-Chop_Busy"
     Target_en_us "The bench is busy"
     Comment "Toast: a car is already on the bench (#415)."
    }
    CustomStringTableItem "{7357AAFD705254CD}" {
     Id "LF-Chop_Collect"
     Target_en_us "Take the cash and parts"
     Comment "Action: collect the payout (#415)."
    }
    CustomStringTableItem "{37FDABA91E5334CF}" {
     Id "LF-Chop_Cooldown"
     Target_en_us "Too much heat. Come back later"
     Comment "Toast: the yard is cooling down (#415)."
    }
    CustomStringTableItem "{0AA160CA1407D632}" {
     Id "LF-Chop_Gone"
     Target_en_us "The car left the yard. The job is off"
     Comment "Toast: the car moved before collection (#415)."
    }
    CustomStringTableItem "{235163142BFE40D6}" {
     Id "LF-Chop_KeyedOwner"
     Target_en_us "That car is yours (you hold its key). Only stolen cars, please."
     Comment "Toast: an owner with the key cannot chop (#415)."
    }
    CustomStringTableItem "{DD4DF16E9EA2A1E6}" {
     Id "LF-Chop_Load"
     Target_en_us "Take this stolen car apart"
     Comment "Action: start dismantling the car in the yard (#415)."
    }
    CustomStringTableItem "{72D3F56BF9D4656A}" {
     Id "LF-Chop_NoRoom"
     Target_en_us "No room for the cash. Make space, then collect."
     Comment "Toast: the worker's inventory cannot take the chop payout, so the car stays (crime review 2026-11-17)."
    }
    CustomStringTableItem "{764914C9C71A4DE3}" {
     Id "LF-Chop_NoVehicle"
     Target_en_us "Park the car in the yard first"
     Comment "Toast: no car in the drop area (#415)."
    }
    CustomStringTableItem "{C8732E51C07DE9D9}" {
     Id "LF-Chop_NoYard"
     Target_en_us "No yard here"
     Comment "Toast: refused (#415)."
    }
    CustomStringTableItem "{DE5E9D7236637A63}" {
     Id "LF-Chop_NotStolen"
     Target_en_us "Only stolen cars. Lockpick or hotwire a car that is not yours and bring it here."
     Comment "Toast: the car is not stolen. Says what counts as hot: a car taken by lockpick or hotwire (#415, #601)."
    }
    CustomStringTableItem "{FDE13A88BF60F4A6}" {
     Id "LF-Chop_Occupied"
     Target_en_us "Get out of the car first"
     Comment "Toast: someone is inside (#415)."
    }
    CustomStringTableItem "{7857599BE396D45B}" {
     Id "LF-Chop_Paid"
     Target_en_us "Paid in cash. The car is gone."
     Comment "Toast: payout taken (#415)."
    }
    CustomStringTableItem "{226E1925807E4ACE}" {
     Id "LF-Chop_Title"
     Target_en_us "Chop shop"
     Comment "Toast title (#415)."
    }
    CustomStringTableItem "{B7117664D8658E3D}" {
     Id "LF-Chop_TooFar"
     Target_en_us "Step up to the bench"
     Comment "Toast: out of reach (#415)."
    }
    CustomStringTableItem "{DB060C732777FC93}" {
     Id "LF-Chop_Work_Fresh"
     Target_en_us "Strip it down: just started"
     Comment "Action: held work on the car, early (#415)."
    }
    CustomStringTableItem "{7334F91AA4AF7DC2}" {
     Id "LF-Chop_Work_Late"
     Target_en_us "Strip it down: almost bare"
     Comment "Action: held work on the car, late (#415)."
    }
    CustomStringTableItem "{96935ECA7C141DFD}" {
     Id "LF-Chop_Work_Mid"
     Target_en_us "Strip it down: half gone"
     Comment "Action: held work on the car, middle (#415)."
    }
    CustomStringTableItem "{8EB5BE0F4B310C71}" {
     Id "LF-Cleanup_Action"
     Target_en_us "Cleanup"
     Comment "F-key action name for cleanup (#690)."
    }
    CustomStringTableItem "{D9E3E533E519382B}" {
     Id "LF-Cleanup_Denied"
     Target_en_us "Cleanup is not available."
     Comment "Greyed-out reason and toast when cleanup is refused (#690)."
    }
    CustomStringTableItem "{343B6E24B7A77DB7}" {
     Id "LF-ClinicShortage_Prices"
     Target_en_us "Medicine"
     Comment "Clinic shortage notice row label: what changed."
    }
    CustomStringTableItem "{89FB7CD603CEB2B2}" {
     Id "LF-ClinicShortage_PricesValue"
     Target_en_us "Prices raised"
     Comment "Clinic shortage notice: medicine prices are higher."
    }
    CustomStringTableItem "{9E13187BE04DD170}" {
     Id "LF-ClinicShortage_Until"
     Target_en_us "Until"
     Comment "Clinic shortage notice row label: how long."
    }
    CustomStringTableItem "{F8B0EA0FB0209C5E}" {
     Id "LF-ClinicShortage_UntilValue"
     Target_en_us "Further notice"
     Comment "Clinic shortage notice: no date is promised."
    }
    CustomStringTableItem "{F3DA3091EFA304D0}" {
     Id "LF-ClinicShortage_Why"
     Target_en_us "Why"
     Comment "Clinic shortage notice row label (#4448)."
    }
    CustomStringTableItem "{7376B8A6E82EC0D4}" {
     Id "LF-ClinicShortage_WhyValue"
     Target_en_us "Stock stolen"
     Comment "Clinic shortage notice: why the pharmacy is closed."
    }
    CustomStringTableItem "{B56342EA30F0AFB8}" {
     Id "LF-ClipStamp_Action"
     Target_en_us "Show clip stamp"
     Comment "Key binding name and toast title for the on-screen clip stamp (#1173)."
    }
    CustomStringTableItem "{FEE030CA6B94E0E5}" {
     Id "LF-ClipStamp_Denied"
     Target_en_us "This server has turned the clip stamp off."
     Comment "Toast when a player presses the clip stamp key and the owner config turned the stamp off (#1173)."
    }
    CustomStringTableItem "{B1D47E2095AC63F8}" {
     Id "LF-ClipStamp_Line"
     Target_en_us "%1  /  %2  /  %3 UTC"
     Comment "The clip stamp text: server name, tail of the player id, real server date and time (UTC). Matches the incident log's clock (#1173)."
    }
    CustomStringTableItem "{434952EDC3418374}" {
     Id "LF-Colour_Beige"
     Target_en_us "Beige"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{F0F8C8C4E7FCFFAC}" {
     Id "LF-Colour_Blue"
     Target_en_us "Blue"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{1A5FAC7D11F4EB3A}" {
     Id "LF-Colour_Brown"
     Target_en_us "Brown"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{5E66599DFE939C4E}" {
     Id "LF-Colour_DarkBlue"
     Target_en_us "Dark blue"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{B618DE1FC7637194}" {
     Id "LF-Colour_DarkBrown"
     Target_en_us "Dark brown"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{5CB9FF47A1C7F19B}" {
     Id "LF-Colour_DarkRed"
     Target_en_us "Dark red"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{4619BC52F54FEE2D}" {
     Id "LF-Colour_Forest"
     Target_en_us "Forest green"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{A0F1A3C81E2E29D2}" {
     Id "LF-Colour_Green"
     Target_en_us "Green"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{6527F4E5B67FD106}" {
     Id "LF-Colour_Grey"
     Target_en_us "Gray"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{D8C60705E728D184}" {
     Id "LF-Colour_LightBlue"
     Target_en_us "Light blue"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{DF8FB55E0BF47747}" {
     Id "LF-Colour_LightGreen"
     Target_en_us "Light green"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{FE5E0C97D593F0D9}" {
     Id "LF-Colour_Orange"
     Target_en_us "Orange"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{8A49425DF7088ABA}" {
     Id "LF-Colour_Red"
     Target_en_us "Red"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{7E4D3C8838195FA1}" {
     Id "LF-Colour_Teal"
     Target_en_us "Teal"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{068873DB84698E08}" {
     Id "LF-Colour_White"
     Target_en_us "White"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{72198C3EB0E55A4C}" {
     Id "LF-Colour_Yellow"
     Target_en_us "Yellow"
     Comment "Car paint colour name, shown on the lot sheet and the respray bay row."
    }
    CustomStringTableItem "{E35B0CC84DBE0C44}" {
     Id "LF-Commendations_None"
     Target_en_us "No commendations yet."
     Comment "Plaque line when nobody has been commended (#4206)."
    }
    CustomStringTableItem "{D241025EB28BCFC9}" {
     Id "LF-Commendations_Read"
     Target_en_us "Read the commendations plaque"
     Comment "Action on the commendations plaque in the station hall (#4206)."
    }
    CustomStringTableItem "{6BABF1BF0291EF96}" {
     Id "LF-Commendations_Title"
     Target_en_us "Commendations"
     Comment "Title of the commendations plaque in the station hall (#4206)."
    }
    CustomStringTableItem "{4E1D7A30B6C2F907}" {
     Id "LF-Consume_Keybind_Throw"
     Target_en_us "Throw the food or drink in your hands"
     Comment "Keybind menu entry for the throw action (default key G, with food or drink in the hands)."
    }
    CustomStringTableItem "{4E1D7A30B6C2F902}" {
     Id "LF-Consume_Refused_Busy"
     Target_en_us "You cannot do that right now."
     Comment "Toast: eating, drinking or throwing refused, no better reason (not a character)."
    }
    CustomStringTableItem "{4E1D7A30B6C2F904}" {
     Id "LF-Consume_Refused_Cuffed"
     Target_en_us "You cannot eat or drink with your hands cuffed."
     Comment "Toast: eating, drinking or throwing refused while restrained."
    }
    CustomStringTableItem "{4E1D7A30B6C2F903}" {
     Id "LF-Consume_Refused_Down"
     Target_en_us "You are in no state to eat or drink."
     Comment "Toast: eating, drinking or throwing refused while dead, downed or knocked out."
    }
    CustomStringTableItem "{4E1D7A30B6C2F905}" {
     Id "LF-Consume_Refused_Vehicle"
     Target_en_us "Get out of the vehicle first."
     Comment "Toast: eating, drinking or throwing refused while seated in a vehicle."
    }
    CustomStringTableItem "{4E1D7A30B6C2F906}" {
     Id "LF-Consume_Refused_Water"
     Target_en_us "You cannot eat or drink while swimming."
     Comment "Toast: eating, drinking or throwing refused while swimming."
    }
    CustomStringTableItem "{4E1D7A30B6C2F901}" {
     Id "LF-Consume_Title"
     Target_en_us "Eating and drinking"
     Comment "Toast title when eating, drinking or throwing food is refused."
    }
    CustomStringTableItem "{5C1E7A3B90D24F68}" {
     Id "LF-Contraband_Cooldown"
     Target_en_us "Your crew needs a breather. Wait a few minutes before the next run."
     Comment "Convoy start refused: the character was paid for a run in the last ten minutes"
    }
    CustomStringTableItem "{1D5E4503B79F1A6F}" {
     Id "LF-Contraband_Crate_Desc"
     Target_en_us "Unstamped bottles, no excise paid. The publican on the square buys them at his back door. Police take it on sight."
     Comment "Item description of the contraband crate"
    }
    CustomStringTableItem "{AC8D0A406C890A6E}" {
     Id "LF-Contraband_Crate_Name"
     Target_en_us "Crate of spirits"
     Comment "Item name of the contraband cargo: a wooden crate of untaxed spirits carried in both hands"
    }
    CustomStringTableItem "{DEDBE2A372F43FF6}" {
     Id "LF-Contraband_HandsFull"
     Target_en_us "Your hands are full. Put down what you carry, then take a crate."
     Comment "Toast when the convoy start cannot hand over the crate because the player already carries something"
    }
    CustomStringTableItem "{07C181E87F83A891}" {
     Id "LF-Contraband_JoinOrg"
     Target_en_us "Contraband runs are for {org} members. Join one or found your own at the {org} desk."
     Comment "Convoy start refused: the player has no organization"
    }
    CustomStringTableItem "{8269943001D9EADB}" {
     Id "LF-Contraband_Loaded"
     Target_en_us "You have a crate. Drive it to the convoy drop, the empty crate cage, and stay in the vehicle until you get there."
     Comment "Toast when the convoy start hands the player a contraband crate at the barn"
    }
    CustomStringTableItem "{B7240E91C5A3D816}" {
     Id "LF-Contraband_NotDriven"
     Target_en_us "The crate has to be driven to the drop. Carrying it over on foot pays nothing."
     Comment "Convoy delivery refused: the run never watched a vehicle, so the crate went on foot"
    }
    CustomStringTableItem "{C6C3575A8274C731}" {
     Id "LF-Contraband_NotHideoutOwner"
     Target_en_us "Your {org} has to hold the hideout before it can run this convoy."
     Comment "Convoy start refused: the player's organization does not hold the hideout"
    }
    CustomStringTableItem "{ADBB1F127BBEB201}" {
     Id "LF-Contraband_Title"
     Target_en_us "Contraband"
     Comment "Toast title for contraband convoy messages"
    }
    CustomStringTableItem "{2F9A63C8D5E1074B}" {
     Id "LF-ContractTracker_Cost"
     Target_en_us "They will remember if you drop this."
     Comment "Contract tracker line (#2914): what walking away costs, in words, never points. Shown when the contract has a stake."
    }
    CustomStringTableItem "{8D40B6E3172A9C58}" {
     Id "LF-ContractTracker_Line"
     Target_en_us "%1: %2"
     Comment "Contract tracker line (#2914): %1 = who it is for and what to do, %2 = the time left in words."
    }
    CustomStringTableItem "{19B4CE7D60A835F2}" {
     Id "LF-ContractTracker_Time_Hurry"
     Target_en_us "hurry"
     Comment "Contract tracker line (#2914): under a fifth of the time is left."
    }
    CustomStringTableItem "{A7D2189E4B6C30F5}" {
     Id "LF-ContractTracker_Time_Plenty"
     Target_en_us "plenty of time"
     Comment "Contract tracker line (#2914): more than half the time is left."
    }
    CustomStringTableItem "{E3518B0FC2A7D694}" {
     Id "LF-ContractTracker_Time_Soon"
     Target_en_us "time is getting on"
     Comment "Contract tracker line (#2914): between a fifth and a half of the time is left."
    }
    CustomStringTableItem "{5C1E7A02B93D4F61}" {
     Id "LF-ContractTracker_Title_Default"
     Target_en_us "Your contract"
     Comment "Contract tracker line (#2914): the title of a contract with no pack row of its own (LF-ContractTracker_Title_<contractId>)."
    }
    CustomStringTableItem "{3565B0AEE39851EE}" {
     Id "LF-Contract_Busy"
     Target_en_us "You already have a contract. Finish that one first."
     Comment "Contract refusal (#2702): one contract at a time per character."
    }
    CustomStringTableItem "{00EEFD92187745AF}" {
     Id "LF-Contract_Cooldown"
     Target_en_us "Nothing for you right now. Come back later."
     Comment "Contract refusal (#2702): the cooldown after the last contract is still running."
    }
    CustomStringTableItem "{874C1FFFA39D4B3B}" {
     Id "LF-Contract_NoRun"
     Target_en_us "The contract could not be tied to your run; it pays as an ordinary run."
     Comment "Contract start (#2702): the run started but the contract did not bind. Should not happen."
    }
    CustomStringTableItem "{FED9A73448625D00}" {
     Id "LF-Contract_NotAble"
     Target_en_us "You are in no state to take on work."
     Comment "Contract refusal (#2702): the player is dead or downed."
    }
    CustomStringTableItem "{03EF818A89F1ED32}" {
     Id "LF-Contract_RunOpen"
     Target_en_us "Finish the run you are on first, then come and see me."
     Comment "Contract refusal (#2913): the player already has a delivery run under way."
    }
    CustomStringTableItem "{A3B9EDA33F8EFD30}" {
     Id "LF-Contract_Started"
     Target_en_us "Right, it is yours. Deliver it in good time and the pay is better than a plain run."
     Comment "Contract start (#2913): the NPC answers a dialogue reply that took a contract; also the toast."
    }
    CustomStringTableItem "{49069B1064628566}" {
     Id "LF-Contract_Unknown"
     Target_en_us "That contract isn't on offer."
     Comment "Contract refusal (#2702): the contract id has no row or no delivery job."
    }
    CustomStringTableItem "{825B941D7F8A5C59}" {
     Id "LF-CounterOutlet_Action"
     Target_en_us "Sell under the counter"
     Comment "Barkeeper action: sell the smuggled cartons and bottles the bar buys (#4200)."
    }
    CustomStringTableItem "{70843034317124F3}" {
     Id "LF-CounterOutlet_Full"
     Target_en_us "That's enough for this week. Come back later."
     Comment "Toast: the bar's demand for the period is spent (#4200)."
    }
    CustomStringTableItem "{506F32AAE67FCF37}" {
     Id "LF-CounterOutlet_Nothing"
     Target_en_us "You have nothing the bar takes."
     Comment "Toast and cannot-perform reason: no goods this bar buys (#4200)."
    }
    CustomStringTableItem "{F63D693CF85C59FE}" {
     Id "LF-CounterOutlet_Police"
     Target_en_us "Not with that badge on, officer."
     Comment "Toast: the barkeeper refuses an on-duty police officer (#4200)."
    }
    CustomStringTableItem "{17E9077EC7547E6D}" {
     Id "LF-CounterOutlet_Sold"
     Target_en_us "Under the counter it goes. You get %1."
     Comment "Toast: goods sold under the counter. %1 = the amount paid (#4200)."
    }
    CustomStringTableItem "{67B2BD45B6D5677C}" {
     Id "LF-CounterOutlet_Title"
     Target_en_us "The barkeeper"
     Comment "Toast title at an under-the-counter outlet (#4200)."
    }
    CustomStringTableItem "{E83AB3CDE17F447B}" {
     Id "LF-Credits_1"
     Target_en_us "PROJECTS WE BUILD ON AND BORROW FROM"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{6BEE352A31EEAE53}" {
     Id "LF-Credits_10"
     Target_en_us "Bohemia Interactive"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{17725D5EE1952883}" {
     Id "LF-Credits_100"
     Target_en_us "GENERATED SOUND EFFECTS (ELEVENLABS)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{27F672D865E0C90E}" {
     Id "LF-Credits_101"
     Target_en_us "Taser sounds: shot, dry fire, cartridge out and in, stun buzz"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{52A89444BDFCFD47}" {
     Id "LF-Credits_102"
     Target_en_us "ElevenLabs text-to-sound-effects, generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{B34B54B3270EA1AB}" {
     Id "LF-Credits_103"
     Target_en_us "GENERATED AUDIO"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{08B073F287287452}" {
     Id "LF-Credits_104"
     Target_en_us "Retail store alarm"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{47D4FB9C4781CDA3}" {
     Id "LF-Credits_105"
     Target_en_us "Hospital robbery alarm"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{FBFB66BA23F58688}" {
     Id "LF-Credits_106"
     Target_en_us "Jailbreak siren"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{53B06789666BA9A0}" {
     Id "LF-Credits_107"
     Target_en_us "Store radio and pub music"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{191CB88166129132}" {
     Id "LF-Credits_108"
     Target_en_us "Music generated with ElevenLabs (ElevenLabs Music, instrumental)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{A8D038048737972B}" {
     Id "LF-Credits_109"
     Target_en_us "Radio track 'Estrada ballad', a slow 92 bpm estrada ballad"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{857E5E05902D8F44}" {
     Id "LF-Credits_11"
     Target_en_us "thebonbon/EveronLife: roleplay chat, mining, jump pad and spike strip code. The mining code was ported with the author's permission given to the maintainer."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{C0DF6FBEA91CB184}" {
     Id "LF-Credits_110"
     Target_en_us "Music generated with ElevenLabs (ElevenLabs Music, instrumental)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{1E14413C1D94DD1F}" {
     Id "LF-Credits_111"
     Target_en_us "Radio track 'Estrada dance', an upbeat estrada dance number"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{96FE8BDEC46005B9}" {
     Id "LF-Credits_112"
     Target_en_us "Music generated with ElevenLabs (ElevenLabs Music, instrumental)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{863CCDCE20D8134D}" {
     Id "LF-Credits_113"
     Target_en_us "Radio track 'Accordion waltz', a 3/4 accordion waltz"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{90202C71BA2045BB}" {
     Id "LF-Credits_114"
     Target_en_us "Music generated with ElevenLabs (ElevenLabs Music, instrumental)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BD650073C9503BC9}" {
     Id "LF-Credits_115"
     Target_en_us "Radio track 'Accordion kolo', a 2/4 Balkan kolo"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BF312B42B05FCAC5}" {
     Id "LF-Credits_116"
     Target_en_us "Music generated with ElevenLabs (ElevenLabs Music, instrumental)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{AD8474D21ADA71ED}" {
     Id "LF-Credits_117"
     Target_en_us "Radio track 'Orchestral morning', a light orchestral morning piece"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{A20FA264480F9153}" {
     Id "LF-Credits_118"
     Target_en_us "Music generated with ElevenLabs (ElevenLabs Music, instrumental)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{8D2FBE7CB9BA1A19}" {
     Id "LF-Credits_119"
     Target_en_us "Radio track 'Orchestral serenade', a Mediterranean serenade"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{0A5B0A916F1F634A}" {
     Id "LF-Credits_12"
     Target_en_us "rekterakathom/EveronLife: parts of the inventory storage manager and the shop rules."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{C06863243F369B82}" {
     Id "LF-Credits_120"
     Target_en_us "Music generated with ElevenLabs (ElevenLabs Music, instrumental)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{034AB24F20B9C523}" {
     Id "LF-Credits_121"
     Target_en_us "Background loops: furnace, sawmill, quarry, farm, still, police desk, chop shop, square, bank murmur, bank till and clock, hospital equipment, hospital ward, payphone line, fuel pump, cash van idle"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{6FE700B6BDC757ED}" {
     Id "LF-Credits_122"
     Target_en_us "ElevenLabs text-to-sound-effects, generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{2B6B443BB2E0DFB9}" {
     Id "LF-Credits_123"
     Target_en_us "Ambience beds: harbor by day"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{0C744196304685EB}" {
     Id "LF-Credits_124"
     Target_en_us "ElevenLabs text-to-sound-effects, loop on, generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{9954645FD5FC4292}" {
     Id "LF-Credits_125"
     Target_en_us "Outpost ambience beds: generator hum, campfire crackle, a radio murmuring in a tent, wind through the nets"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{C35B3ADBB9D89721}" {
     Id "LF-Credits_126"
     Target_en_us "ElevenLabs text-to-sound-effects, loop on, generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{9968FD984CF4FABF}" {
     Id "LF-Credits_127"
     Target_en_us "GENERATED VOICES"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{9604C54EE7C42A8F}" {
     Id "LF-Credits_128"
     Target_en_us "Town clerk's spoken lines: all thirteen, the old arrival lines and the newcomer's welcome"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{2A31B23D96B99646}" {
     Id "LF-Credits_129"
     Target_en_us "ElevenLabs text-to-speech, voice 'Julien - Serious' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{4698D3AC325BC21D}" {
     Id "LF-Credits_13"
     Target_en_us "Jacob-Mango/EveronLife: parts of the quantity component."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{EF83CE5E501404F6}" {
     Id "LF-Credits_130"
     Target_en_us "Cafe owner's seven lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{2E46C1E4691E2B33}" {
     Id "LF-Credits_131"
     Target_en_us "ElevenLabs text-to-speech, voice 'Mariva Viva Muse - Warm and Energetic' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{A636049B1615FBC6}" {
     Id "LF-Credits_132"
     Target_en_us "Gendarme's three lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{16186E0A5EB3A7AA}" {
     Id "LF-Credits_133"
     Target_en_us "ElevenLabs text-to-speech, voice 'Cedric - Calm' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{FE189EB428AF142A}" {
     Id "LF-Credits_134"
     Target_en_us "Dispatcher's 27 radio lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{4D04FB45B8DBCE90}" {
     Id "LF-Credits_135"
     Target_en_us "ElevenLabs text-to-speech, voice 'Eric - Smooth, Trustworthy' from the ElevenLabs premade voices. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{282E8AC7953417F3}" {
     Id "LF-Credits_136"
     Target_en_us "Dockworkers' four lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{A096E2717EAFA002}" {
     Id "LF-Credits_137"
     Target_en_us "ElevenLabs text-to-speech, voice 'Delgado Stephane' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{A8B2E6676A5FC762}" {
     Id "LF-Credits_138"
     Target_en_us "Fishmonger's four lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{4165A33B0CDDFEFA}" {
     Id "LF-Credits_139"
     Target_en_us "ElevenLabs text-to-speech, voice 'Chris - Charming, Down-to-Earth' from the ElevenLabs premade voices. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{CA6300164F63F92D}" {
     Id "LF-Credits_14"
     Target_en_us "CONTRIBUTORS"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{ADFB88278344FB5B}" {
     Id "LF-Credits_140"
     Target_en_us "Chandler's four lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{856667E0087DE2BB}" {
     Id "LF-Credits_141"
     Target_en_us "ElevenLabs text-to-speech, voice 'George - Warm, Captivating Storyteller' from the ElevenLabs premade voices. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{A8A3DCF36786BA71}" {
     Id "LF-Credits_142"
     Target_en_us "Passers-by's four lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{740D77BACEA78D9C}" {
     Id "LF-Credits_143"
     Target_en_us "ElevenLabs text-to-speech, voice 'Sylvain - International english' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{3CA96CAAEC9AB85D}" {
     Id "LF-Credits_144"
     Target_en_us "Relocated walkers' four lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{66D2E150BBB2BBDF}" {
     Id "LF-Credits_145"
     Target_en_us "ElevenLabs text-to-speech, voice 'Real Jeremy - Conversational' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{92C2B4F451409735}" {
     Id "LF-Credits_146"
     Target_en_us "Anselme's nine lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{E884C0110C5B632F}" {
     Id "LF-Credits_147"
     Target_en_us "ElevenLabs text-to-speech, voice 'Bacbuc - Audiobooks' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{427FC7360D73D3A9}" {
     Id "LF-Credits_148"
     Target_en_us "Marius Caron's nine lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{CBA36637CDF4AE7F}" {
     Id "LF-Credits_149"
     Target_en_us "ElevenLabs text-to-speech, voice 'Alex - Warm, Empathetic, Confident' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{1E61840F28B6ADBC}" {
     Id "LF-Credits_15"
     Target_en_us "Everyone who has committed to the repository is on the GitHub contributors list, counted from the commit history."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{D71B88C351B4EA69}" {
     Id "LF-Credits_150"
     Target_en_us "Pierre Vasseur's"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{CDA2F516388DB3A1}" {
     Id "LF-Credits_151"
     Target_en_us "ElevenLabs text-to-speech, voice 'Kevin Nitcheu' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{A4F5C98AFC7B32E9}" {
     Id "LF-Credits_152"
     Target_en_us "Lucien Roux's"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{6D1FD16D6C772E09}" {
     Id "LF-Credits_153"
     Target_en_us "ElevenLabs text-to-speech, voice 'JP - Social media' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{1519D7ABE5844759}" {
     Id "LF-Credits_154"
     Target_en_us "Shopkeepers' five lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{312C6E9E98AFEA2C}" {
     Id "LF-Credits_155"
     Target_en_us "ElevenLabs text-to-speech, voice 'James - French Podcast' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{B4E1994EB6122A72}" {
     Id "LF-Credits_156"
     Target_en_us "Store clerks' five role lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{E9299E95D9B8F174}" {
     Id "LF-Credits_157"
     Target_en_us "ElevenLabs text-to-speech, voice 'Jamie - French Accent & Charismatic' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{4444E26E9DCF449C}" {
     Id "LF-Credits_158"
     Target_en_us "Bank teller's five role lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{AC305FCEA4FFD18B}" {
     Id "LF-Credits_159"
     Target_en_us "ElevenLabs text-to-speech, voice 'Nicolas - French English Male' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{59CAD5C15A66EDA7}" {
     Id "LF-Credits_16"
     Target_en_us "LICENSES"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{0625D39270A113DC}" {
     Id "LF-Credits_160"
     Target_en_us "Desk officer's five role lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{6A3DD60E0145C890}" {
     Id "LF-Credits_161"
     Target_en_us "ElevenLabs text-to-speech, voice 'Benjamin - Deep and Calm' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{09BDBEC3C3443456}" {
     Id "LF-Credits_162"
     Target_en_us "Clinic doctor's five role lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{0AB81D75DD0A7FCC}" {
     Id "LF-Credits_163"
     Target_en_us "ElevenLabs text-to-speech, voice 'Antoine - Articulate, Calm and Bright' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{C489B5F7558670CF}" {
     Id "LF-Credits_164"
     Target_en_us "Mechanics' five role lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{2FA0C88C63E929C7}" {
     Id "LF-Credits_165"
     Target_en_us "ElevenLabs text-to-speech, voice 'Loic' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{D24D05840A3D9820}" {
     Id "LF-Credits_166"
     Target_en_us "The fence's five role lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{473EC6052D855BB0}" {
     Id "LF-Credits_167"
     Target_en_us "ElevenLabs text-to-speech, voice 'Lucas - Deep, Raspy and Natural' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{4ED77DE7DEE2573C}" {
     Id "LF-Credits_168"
     Target_en_us "Town civilians' five role lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{0774CCE82D802254}" {
     Id "LF-Credits_169"
     Target_en_us "ElevenLabs text-to-speech, voice 'Eliot - Relaxed & Casual' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{AD56C627EA2AD519}" {
     Id "LF-Credits_17"
     Target_en_us "The source in this repository is MIT. Contributions are licensed under MIT."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{6FDBFD431F29A691}" {
     Id "LF-Credits_170"
     Target_en_us "The market trader's five role lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{24B6F60FC64E50FE}" {
     Id "LF-Credits_171"
     Target_en_us "ElevenLabs text-to-speech, voice 'Nicolas Estrem - Mature' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{8B3125F791403A21}" {
     Id "LF-Credits_172"
     Target_en_us "Night-shift worker's spoken lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{6DEF465ED9803D1A}" {
     Id "LF-Credits_173"
     Target_en_us "ElevenLabs text-to-speech, voice 'Raspy - Convincing and Weary Narrator' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{331934ECDCED4E47}" {
     Id "LF-Credits_174"
     Target_en_us "Old man Gaspard Lenoir's six lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{B9BF5DC2926087D1}" {
     Id "LF-Credits_175"
     Target_en_us "ElevenLabs text-to-speech, voice 'Alder - Warm, Raspy and Comforting' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{CC2101DE9FC8F7B8}" {
     Id "LF-Credits_176"
     Target_en_us "Go-between Hugo Valmy's five spoken lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{C9C86A5A1ED40DCE}" {
     Id "LF-Credits_177"
     Target_en_us "ElevenLabs text-to-speech, voice 'Cyril - French-Accented Narrator' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{8A4563FF1C5E590F}" {
     Id "LF-Credits_178"
     Target_en_us "Tally clerk Marcel Brun's five spoken lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{1FC189AF5F7BE2CC}" {
     Id "LF-Credits_179"
     Target_en_us "ElevenLabs text-to-speech, voice 'Antonin - Warm, Unsettling, Elegant' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{3A24A564BBC40F8F}" {
     Id "LF-Credits_18"
     Target_en_us "The build published on the Arma Reforger Workshop is licensed under the Arma Public License (APL), because the Workshop Terms of Use require an APL-family license for uploaded content."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BDA999D625B19F3D}" {
     Id "LF-Credits_180"
     Target_en_us "Coal yard clerk Gaston Roux's three spoken lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{A0A7AF015F2F74D4}" {
     Id "LF-Credits_181"
     Target_en_us "ElevenLabs text-to-speech, voice 'James - Relaxed American Voice' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{0D1A861CFC75EB60}" {
     Id "LF-Credits_182"
     Target_en_us "Barkeeper Josselin Vidal's five spoken lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{861D6EE9C532FE97}" {
     Id "LF-Credits_183"
     Target_en_us "ElevenLabs text-to-speech, voice 'Hank Harvey - Gruff, Rugged and Wise' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{FD0F462ADB68D02E}" {
     Id "LF-Credits_184"
     Target_en_us "Registrar Gaspard Perrin's six spoken lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{00BFA04619043DFC}" {
     Id "LF-Credits_185"
     Target_en_us "ElevenLabs text-to-speech, voice 'Liam - Energetic, Social Media Creator' (premade). Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{90696CB2BFCA919F}" {
     Id "LF-Credits_186"
     Target_en_us "Harbor master Marcel Roux's fourteen spoken lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{13C921E553E1333A}" {
     Id "LF-Credits_187"
     Target_en_us "ElevenLabs text-to-speech, voice 'LaRauque - Hoarse, Deep, and Dramatic' from the ElevenLabs voice library. Voice generated with ElevenLabs"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{1FD2D69733781B26}" {
     Id "LF-Credits_188"
     Target_en_us "MUSIC"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{413CD254F961C2CC}" {
     Id "LF-Credits_189"
     Target_en_us "'Carefree'"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{9B225E9F684B34BE}" {
     Id "LF-Credits_19"
     Target_en_us "Third-party code keeps its own license and notice. Third-party assets so far are only the CC0 texture sets under 'Textures (CC0)'."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{FB7F01DA30ED5462}" {
     Id "LF-Credits_190"
     Target_en_us "Kevin MacLeod (incompetech.com)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{20D09EC5580FB73B}" {
     Id "LF-Credits_191"
     Target_en_us "'Hitman'"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{EB138655CB017E1B}" {
     Id "LF-Credits_192"
     Target_en_us "Kevin MacLeod (incompetech.com)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{ADAAD0ACB3D62D04}" {
     Id "LF-Credits_193"
     Target_en_us "VOICE-OVER"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{4BC652FE250FB8F9}" {
     Id "LF-Credits_194"
     Target_en_us "Piper"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{B19D39615671DC17}" {
     Id "LF-Credits_195"
     Target_en_us "Michael Hansen / Rhasspy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{031A67A6561231CB}" {
     Id "LF-Credits_196"
     Target_en_us "Voice en_US-ljspeech-high"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{41B72E8D01F8E68E}" {
     Id "LF-Credits_197"
     Target_en_us "Trained from scratch on LJ Speech (Keith Ito)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{899288C00EFEEAC6}" {
     Id "LF-Credits_198"
     Target_en_us "FONTS AND LOGO"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{AB6DB02C0E62EDEF}" {
     Id "LF-Credits_199"
     Target_en_us "Anniversary priest Father Étienne Morel's memorial speech"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{CFC4ECAEFB5E7DFF}" {
     Id "LF-Credits_2"
     Target_en_us "Everon Life RPG Framework"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{07EEDCD277D92D50}" {
     Id "LF-Credits_20"
     Target_en_us "TOOLS"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{98645FC8B2D9330E}" {
     Id "LF-Credits_200"
     Target_en_us "Gaspard Lenoir's two anniversary letter lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{31D05720B5CD5B48}" {
     Id "LF-Credits_201"
     Target_en_us "LIVERIES"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{4D22C5E97B2689AA}" {
     Id "LF-Credits_202"
     Target_en_us "UAZ469 police livery"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{D12E84B0D1EB5D48}" {
     Id "LF-Credits_203"
     Target_en_us "S105 police livery"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{E5F5B4D1BBEFC2E5}" {
     Id "LF-Credits_204"
     Target_en_us "UAZ469 rebel livery"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{8DF88C3D0E2242BE}" {
     Id "LF-Credits_205"
     Target_en_us "Ural4320 rebel livery"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BE5E5996E3159D99}" {
     Id "LF-Credits_206"
     Target_en_us "S1203 police livery"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{3357BDF3A3C49CCF}" {
     Id "LF-Credits_207"
     Target_en_us "UAZ452 ambulance livery"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{C1690BAB498831CF}" {
     Id "LF-Credits_208"
     Target_en_us "S1203 ambulance livery"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{92B8B9DD3342AC1A}" {
     Id "LF-Credits_209"
     Target_en_us "S105 taxi livery"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{09A1E735EF193A11}" {
     Id "LF-Credits_21"
     Target_en_us "Arma Reforger Tools and the Workbench IDE by Bohemia Interactive build and test the mod."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BCC6DE3AE86E2A44}" {
     Id "LF-Credits_210"
     Target_en_us "S1203 company bakery livery"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{B77F496979C34298}" {
     Id "LF-Credits_211"
     Target_en_us "S1203 company factory livery"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{7E19D38663FDACAA}" {
     Id "LF-Credits_212"
     Target_en_us "UAZ452 company hardware livery"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{D8C2792E870AA9C3}" {
     Id "LF-Credits_213"
     Target_en_us "Ural4320 company timber livery"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{F36B1E4FCF515481}" {
     Id "LF-Credits_214"
     Target_en_us "CONTRABAND LABELS"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{1A410F6815AF10B2}" {
     Id "LF-Credits_215"
     Target_en_us "Cigarette carton label"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BD8D70A1D99DD608}" {
     Id "LF-Credits_216"
     Target_en_us "Vodka bottle label"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{0AC90225C1A0D9FD}" {
     Id "LF-Credits_217"
     Target_en_us "WORK-SITE SOUNDS"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{C2A33E2596D13DB2}" {
     Id "LF-Credits_218"
     Target_en_us "Work-site sounds: shift whistle, mine tool bed, sawmill tool bed"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{C6A1555A111852AA}" {
     Id "LF-Credits_219"
     Target_en_us "Desk sergeant's five lines"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{B8555A0EF2D6690B}" {
     Id "LF-Credits_22"
     Target_en_us "The clothing pipeline runs Bohemia Interactive's Enfusion Blender Tools (GPL-3.0) from the local Arma Reforger Tools install."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{D2E4EF8E85E93FA3}" {
     Id "LF-Credits_220"
     Target_en_us "Booking typewriter and ink pad stamp sounds"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{2BF516A298D246D4}" {
     Id "LF-Credits_221"
     Target_en_us "The north outpost's spoken lines: quartermaster"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{53B28D555A161361}" {
     Id "LF-Credits_23"
     Target_en_us "ArchBuilder by Arribas, a Blender add-on, generates the buildings. Each user runs their own licensed copy; none of its source, keys or buyer details are in the repo."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{43162672CA02A922}" {
     Id "LF-Credits_24"
     Target_en_us "Piper makes the trailer voice-over (see Voice-over below)."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{3F64A1C030B6AE18}" {
     Id "LF-Credits_25"
     Target_en_us "The website is Vite, React, TypeScript and Tailwind, hosted on Cloudflare Pages, built from this repository and GitHub."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{B3FFF0D2F5696CB3}" {
     Id "LF-Credits_26"
     Target_en_us "Life Framework is built with AI coding agents under the maintainer's direction, on a Claude subscription."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{74FEA389AE2630A6}" {
     Id "LF-Credits_27"
     Target_en_us "GENERATED 3D ASSETS"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{4D47E82197EE184B}" {
     Id "LF-Credits_28"
     Target_en_us "Taser model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{5470B26BCCCFFD50}" {
     Id "LF-Credits_29"
     Target_en_us "Meshy text-to-3D, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BB787B4246452E5C}" {
     Id "LF-Credits_3"
     Target_en_us "Arkensor and its contributors"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{3FB703A789E0C8B2}" {
     Id "LF-Credits_30"
     Target_en_us "Handcuffs model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{CDF5AA0DB3987497}" {
     Id "LF-Credits_31"
     Target_en_us "Modeled in Blender (articulated strands; the first version was generated with Meshy), textures baked from procedural nodes"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{A138B1E6121712F3}" {
     Id "LF-Credits_32"
     Target_en_us "Vault drill model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{864246B648D44A9E}" {
     Id "LF-Credits_33"
     Target_en_us "Meshy text-to-image, then Meshy image-to-3D, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{C23117FC3D0CE6E0}" {
     Id "LF-Credits_34"
     Target_en_us "Bank vault door and frame detail"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{3811717078DB0F62}" {
     Id "LF-Credits_35"
     Target_en_us "Meshy image-to-image and image-to-3D, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{12D57416F4266B17}" {
     Id "LF-Credits_36"
     Target_en_us "Spike strip model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BFC9A25C7430FA5A}" {
     Id "LF-Credits_37"
     Target_en_us "Meshy text-to-3D, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{6DB121D762F94EB6}" {
     Id "LF-Credits_38"
     Target_en_us "Cash strongbox model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{F29C51000776C6AB}" {
     Id "LF-Credits_39"
     Target_en_us "Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BC3386AB2D3DCC03}" {
     Id "LF-Credits_4"
     Target_en_us "Overthrow"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{450CA04B3D097A32}" {
     Id "LF-Credits_40"
     Target_en_us "Lockpick model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{AD63B637AFDC04F0}" {
     Id "LF-Credits_41"
     Target_en_us "Modeled in Blender (a leather wallet, hook pick and tension wrench; the first version was generated with Meshy), textures baked from procedural nodes"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{B770ED13520D3FD0}" {
     Id "LF-Credits_42"
     Target_en_us "Balaclava"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BACD20FE558CCF0E}" {
     Id "LF-Credits_43"
     Target_en_us "Modeled in Blender: cut from the head of Bohemia's base body (Body_LOD0 in the Arma Reforger Samples, downloaded at run time and never committed), pushed out 6 mm with an eye opening."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{92F893564225C354}" {
     Id "LF-Credits_44"
     Target_en_us "Doll mask"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{086D86A2EDA2B8CF}" {
     Id "LF-Credits_45"
     Target_en_us "Modeled in Blender: cut from the head of Bohemia's base body (Body_LOD0 in the Arma Reforger Samples, downloaded at run time and never committed), held clear of the skin."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{2979BA666C42667B}" {
     Id "LF-Credits_46"
     Target_en_us "Hockey mask"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{F035DFEB809CA450}" {
     Id "LF-Credits_47"
     Target_en_us "Modeled in Blender (cut from the Samples body head as above, with eye slits and an elastic strap); the faded paint, vent holes and cracks are drawn by the same script (numpy)."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{067B4509CC39E039}" {
     Id "LF-Credits_48"
     Target_en_us "1989 general-store food and drink models"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{D3F401496BAE82D2}" {
     Id "LF-Credits_49"
     Target_en_us "Modeled in Blender (plain color wrappers, no text or brands), textures baked from procedural nodes; the bread and soda water items use the base game meshes S2_Bread and SodaBottle_01 (a siphon)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{024DCF01977FDC8E}" {
     Id "LF-Credits_5"
     Target_en_us "Aaron Static, the maintainer of Overthrow"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{F730C1A960A78829}" {
     Id "LF-Credits_50"
     Target_en_us "Bakery and butcher foods"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{F30094F417F14EDD}" {
     Id "LF-Credits_51"
     Target_en_us "Modeled in Blender (plain colors baked from procedural nodes, no text or brands); the ham joint item uses the base game mesh MeatLeg_01"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BC78749D66D5773D}" {
     Id "LF-Credits_52"
     Target_en_us "Fishing rod, bait tin and trout models"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BD94432B1728051F}" {
     Id "LF-Credits_53"
     Target_en_us "Modeled in Blender (the small and large trout are the same script, scaled), textures baked from procedural nodes"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{AE4C658004027BAA}" {
     Id "LF-Credits_54"
     Target_en_us "Fishing float model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{280AE9553E12DCF0}" {
     Id "LF-Credits_55"
     Target_en_us "Modeled in Blender (a red-and-white round bobber on a quill), texture baked from procedural nodes"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{59A5E16ECC87C9D1}" {
     Id "LF-Credits_56"
     Target_en_us "Fish models: mackerel, bass, tuna, perch, carp, each in three sizes"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{127400A67F429364}" {
     Id "LF-Credits_57"
     Target_en_us "Meshy text-to-3D (preview, refine, remesh), generated with Meshy; no CC0 or CC-BY fish fit, so the library rungs came first and were skipped"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{315390E10DFAFDB2}" {
     Id "LF-Credits_58"
     Target_en_us "Evidence marker model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{3A91C10B099318FB}" {
     Id "LF-Credits_59"
     Target_en_us "Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{45A798B997EBD08B}" {
     Id "LF-Credits_6"
     Target_en_us "LNGCLifeRP_Public"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{FD7062FD324F25B7}" {
     Id "LF-Credits_60"
     Target_en_us "Pager model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{80D21A43FAC2B85A}" {
     Id "LF-Credits_61"
     Target_en_us "Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{708E4EC4AFBEC67D}" {
     Id "LF-Credits_62"
     Target_en_us "Money bag model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{3AC68A94A8103570}" {
     Id "LF-Credits_63"
     Target_en_us "Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{D08F14333F89DB8C}" {
     Id "LF-Credits_64"
     Target_en_us "Apple juicer model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{3DC31C741325E00A}" {
     Id "LF-Credits_65"
     Target_en_us "Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{6973D488BECA5042}" {
     Id "LF-Credits_66"
     Target_en_us "Police scanner model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{3CAE93E3CE89F176}" {
     Id "LF-Credits_67"
     Target_en_us "Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{250D4D6A466ED5C6}" {
     Id "LF-Credits_68"
     Target_en_us "Post parcel model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{115DDAA18230398D}" {
     Id "LF-Credits_69"
     Target_en_us "Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{0A506CD8FC4C3064}" {
     Id "LF-Credits_7"
     Target_en_us "LNGCGenetic"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{AF97E8FC2EF2C69E}" {
     Id "LF-Credits_70"
     Target_en_us "Moonshine still model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{C96DB5CDAEB0CD3B}" {
     Id "LF-Credits_71"
     Target_en_us "Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{F31FEA189E5AF4EA}" {
     Id "LF-Credits_72"
     Target_en_us "Smelter furnace model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{46595D86CD131263}" {
     Id "LF-Credits_73"
     Target_en_us "Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{AFAD89EA248BB053}" {
     Id "LF-Credits_74"
     Target_en_us "Dispatch console model"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{FE16DFCBBA5532B4}" {
     Id "LF-Credits_75"
     Target_en_us "Meshy image-to-3D from a Meshy text-to-image concept, generated with Meshy"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{4BB050278AF6FC4B}" {
     Id "LF-Credits_76"
     Target_en_us "TEXTURES (CC0)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{C3252F897790C80D}" {
     Id "LF-Credits_77"
     Target_en_us "Metal 046 B"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{50AF76D0FD61586D}" {
     Id "LF-Credits_78"
     Target_en_us "ambientCG"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{F5CCE61786E1F741}" {
     Id "LF-Credits_79"
     Target_en_us "Metal 047 B"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{2CC9132CA7FA3223}" {
     Id "LF-Credits_8"
     Target_en_us "ArchBuilder"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{450BEEBC033F072C}" {
     Id "LF-Credits_80"
     Target_en_us "ambientCG"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{F386EBE00044EE4B}" {
     Id "LF-Credits_81"
     Target_en_us "Metal 048 B"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{CFAB4F5659DE4528}" {
     Id "LF-Credits_82"
     Target_en_us "ambientCG"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{01785E5C9E42AE6E}" {
     Id "LF-Credits_83"
     Target_en_us "Metal 058 C"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{50C157F2EB525B36}" {
     Id "LF-Credits_84"
     Target_en_us "ambientCG"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{62B659557E7B6464}" {
     Id "LF-Credits_85"
     Target_en_us "Rust 009"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{BB3735C9B27F3750}" {
     Id "LF-Credits_86"
     Target_en_us "ambientCG"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{135C13CA084B8504}" {
     Id "LF-Credits_87"
     Target_en_us "Rock 035"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{F7B8FCDCE0544A88}" {
     Id "LF-Credits_88"
     Target_en_us "ambientCG"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{7320D160EB0096E2}" {
     Id "LF-Credits_89"
     Target_en_us "Rock 058"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{99FF09A0FE490E2D}" {
     Id "LF-Credits_9"
     Target_en_us "Arribas"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{AE8C2BFD5BDF0B32}" {
     Id "LF-Credits_90"
     Target_en_us "ambientCG"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{3D8F4A1805394D57}" {
     Id "LF-Credits_91"
     Target_en_us "Metal 038"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{8501F8EBAEAC730A}" {
     Id "LF-Credits_92"
     Target_en_us "ambientCG"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{D633223051CC74DB}" {
     Id "LF-Credits_93"
     Target_en_us "Leather 014"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{359938F793945EA0}" {
     Id "LF-Credits_94"
     Target_en_us "ambientCG"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{9D89028E9844FF11}" {
     Id "LF-Credits_95"
     Target_en_us "POLY HAVEN MODELS (CC0)"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{D3080A6969C70816}" {
     Id "LF-Credits_96"
     Target_en_us "Cement Bag"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{AA282A4B5F344509}" {
     Id "LF-Credits_97"
     Target_en_us "PierreB3D"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{FA45D273D68AA84A}" {
     Id "LF-Credits_98"
     Target_en_us "LIBRARY AND PURCHASED MODELS"
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{C38448870F6795CB}" {
     Id "LF-Credits_99"
     Target_en_us "Models taken from the open libraries (CC0, CC-BY) or bought by a human on CGTrader and similar markets (Royalty Free), then run through clean and import."
     Comment "Generated from docs/credits.md by tools\cli credits (#481)"
    }
    CustomStringTableItem "{8B6C65DD4369872B}" {
     Id "LF-Credits_Close"
     Target_en_us "CLOSE"
     Comment "Close button of the credits screen (#481)."
    }
    CustomStringTableItem "{9582A7800A1EB28A}" {
     Id "LF-Credits_Intro"
     Target_en_us "Who Life Framework is built on."
     Comment "Line under the credits title (#481)."
    }
    CustomStringTableItem "{79A220F1E2158419}" {
     Id "LF-Credits_Next"
     Target_en_us "NEXT"
     Comment "Next-page button of the credits screen (#481)."
    }
    CustomStringTableItem "{4F2F9090FCEBA8F1}" {
     Id "LF-Credits_Page"
     Target_en_us "Page %1 of %2"
     Comment "Page counter of the credits screen (#481)."
    }
    CustomStringTableItem "{28D3883FADB50BF9}" {
     Id "LF-Credits_Prev"
     Target_en_us "PREVIOUS"
     Comment "Previous-page button of the credits screen (#481)."
    }
    CustomStringTableItem "{803147BD364F1908}" {
     Id "LF-Credits_Title"
     Target_en_us "CREDITS"
     Comment "Heading of the in-game credits screen (#481)."
    }
    CustomStringTableItem "{67770498C05C0FBE}" {
     Id "LF-CrewRob_ActionHold"
     Target_en_us "Hold the till"
     Comment "Action name on a crew till: the holder keeps the spot while the take grows. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{8EB8592BEBD4AE2D}" {
     Id "LF-CrewRob_ActionStart"
     Target_en_us "Start the job"
     Comment "Action name on a crew till: forms the crew and sounds the alarm. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{457F6AD0EAA7943A}" {
     Id "LF-CrewRob_ActionTake"
     Target_en_us "Take the money"
     Comment "Action name on a crew till: the holder ends the job and the crew is paid. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{C322408C87718678}" {
     Id "LF-CrewRob_Failed"
     Target_en_us "The job fell through."
     Comment "Toast to the crew when the holder went down, was cuffed or left. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{84917ADCBAB1B882}" {
     Id "LF-CrewRob_Holding"
     Target_en_us "You hold the till. Stay on the spot while the take grows."
     Comment "Toast to the holder when they begin holding the till. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{D3D593ECA8A52107}" {
     Id "LF-CrewRob_Paid"
     Target_en_us "Your cut: %1"
     Comment "Toast to a crew member paid a share. %1 = the amount. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{76F2D351B5B1A676}" {
     Id "LF-CrewRob_PoliceNear"
     Target_en_us "Lookout: an officer is coming!"
     Comment "Toast to the lookouts when an on-duty officer comes within range of the till. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{46FA120726B4AAC4}" {
     Id "LF-CrewRob_Refused_Busy"
     Target_en_us "A job is already running at this till."
     Comment "Toast: a crew job cannot start, one is running. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{23A32E84E4438425}" {
     Id "LF-CrewRob_Refused_Crew"
     Target_en_us "You need armed partners close by to start a job."
     Comment "Toast: too few armed crew to start a crew job. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{3BA8E7838D3118D9}" {
     Id "LF-CrewRob_Refused_Holding"
     Target_en_us "You are already holding the till."
     Comment "Toast: hold pressed twice. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{87D05614C8D3B0D4}" {
     Id "LF-CrewRob_Refused_NoJob"
     Target_en_us "No job is running here."
     Comment "Toast: hold or take pressed with no job running. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{2AF6238C7A3DFF23}" {
     Id "LF-CrewRob_Refused_NotHolder"
     Target_en_us "Only the one who started the job can do that."
     Comment "Toast: a lookout pressed the holder's action. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{F7C397C4E7B78F30}" {
     Id "LF-CrewRob_Refused_Nothing"
     Target_en_us "There is nothing to take yet. Hold the till first."
     Comment "Toast: take pressed with an empty take. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{21B51EAAD8601D52}" {
     Id "LF-CrewRob_Started"
     Target_en_us "The job is on. The holder holds the till; stay close."
     Comment "Toast to every crew member when a crew till robbery starts. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{0E341FD4A5F997EA}" {
     Id "LF-CrewRob_Title"
     Target_en_us "Crew job"
     Comment "Toast title of the crew till robbery. (Feature/Crime/LF_CrewRobActions.c)"
    }
    CustomStringTableItem "{7E4AE1269E743655}" {
     Id "LF-Crime_RobCar_Desc"
     Target_en_us "Steal an armored car"
     Comment "Annex vehicle theft action description"
    }
    CustomStringTableItem "{D79CB0009B448B8F}" {
     Id "LF-Crime_RobCar_Name"
     Target_en_us "Steal the vehicle"
     Comment "Annex vehicle theft action name"
    }
    CustomStringTableItem "{A83DB0C583355AC6}" {
     Id "LF-Crime_RobRack_Desc"
     Target_en_us "Take a rifle from the rack"
     Comment "Annex weapon rack robbery action description"
    }
    CustomStringTableItem "{7D681F35B1A3C6FE}" {
     Id "LF-Crime_RobRack_Name"
     Target_en_us "Rob the weapon rack"
     Comment "Annex weapon rack robbery action name"
    }
    CustomStringTableItem "{D508B7722F72EE9F}" {
     Id "LF-Crime_RobSafe_Desc"
     Target_en_us "Take $200 from the safe"
     Comment "Annex safe robbery action description"
    }
    CustomStringTableItem "{2605E472E34A7E80}" {
     Id "LF-Crime_RobSafe_Name"
     Target_en_us "Rob the safe"
     Comment "Annex safe robbery action name"
    }
    CustomStringTableItem "{3152A1C0B7D34E02}" {
     Id "LF-Death_CostBill"
     Target_en_us "The bill is {currency:sym}%1."
     Comment "Death screen cost line: the hospital bill; %1 is the amount from the respawn config, left out when it is 0 (#3152)."
    }
    CustomStringTableItem "{3152A1C0B7D34E03}" {
     Id "LF-Death_CostKeeps"
     Target_en_us "What you carried stays with your body."
     Comment "Death screen cost line: the dead body keeps the carried items (#3152)."
    }
    CustomStringTableItem "{3152A1C0B7D34E01}" {
     Id "LF-Death_CostWake"
     Target_en_us "You wake at the hospital."
     Comment "Death screen cost line: where a dead player wakes (#3152)."
    }
    CustomStringTableItem "{EF124E70F7C349D7}" {
     Id "LF-DebugJailMe_Action"
     Target_en_us "Send me to jail (playtest)"
     Comment "Debug-only board action outside the jail: jails you for 60 s."
    }
    CustomStringTableItem "{D8050889F5E6497E}" {
     Id "LF-DebugMenu_ResetWorld"
     Target_en_us "Reset world"
     Comment "Debug menu: first press of the reset-world button (asks again before it resets)."
    }
    CustomStringTableItem "{42E123AF9D0648DA}" {
     Id "LF-DebugMenu_ResetWorldSure"
     Target_en_us "Press again: reset for everyone"
     Comment "Debug menu: reset-world button after the first press."
    }
    CustomStringTableItem "{94AAB43F98726DB0}" {
     Id "LF-Dialogue_Clerk_AboutTown"
     Target_en_us "It is a small town. People here know each other, and they remember faces. Stay honest and you will do well."
     Comment "Dialogue caption, spoken by the clerk NPC (voiced; tools/assets/dialogue/clerk). Changing it needs: tools\\cli asset dialogue clerk"
    }
    CustomStringTableItem "{CC089999560B9785}" {
     Id "LF-Dialogue_Clerk_AboutWork"
     Target_en_us "The post office always needs couriers. For anything else, read the town board. It lists who is hiring."
     Comment "Dialogue caption, spoken by the clerk NPC (voiced; tools/assets/dialogue/clerk). Changing it needs: tools\\cli asset dialogue clerk"
    }
    CustomStringTableItem "{D90DCDEF90A1C933}" {
     Id "LF-Dialogue_Clerk_Bank"
     Target_en_us "You were paid in cash. Cash in a pocket is lost if you die. Money in the bank is not. Deposit some at the cash machine on the bank wall, next to the post office."
     Comment "Dialogue caption, spoken by the clerk NPC (voiced; tools/assets/dialogue/clerk). Changing it needs: tools\\cli asset dialogue clerk"
    }
    CustomStringTableItem "{5DFED210FA00C577}" {
     Id "LF-Dialogue_Clerk_Board"
     Target_en_us "Now read the town board on the square, right beside me. It shows where everything in town is."
     Comment "Dialogue caption, spoken by the clerk NPC (voiced; tools/assets/dialogue/clerk). Changing it needs: tools\\cli asset dialogue clerk"
    }
    CustomStringTableItem "{93CF95EE9BA6015C}" {
     Id "LF-Dialogue_Clerk_Bye"
     Target_en_us "Good day to you."
     Comment "Dialogue caption, spoken by the clerk NPC (voiced; tools/assets/dialogue/clerk). Changing it needs: tools\\cli asset dialogue clerk"
    }
    CustomStringTableItem "{08B7665F0C1963C1}" {
     Id "LF-Dialogue_Clerk_Done"
     Target_en_us "Welcome. Here is something to start you off. The town is yours now. The shops, the car lot, the police station and the organization office are all on the town board."
     Comment "Dialogue caption, spoken by the clerk NPC (voiced; tools/assets/dialogue/clerk). Changing it needs: tools\\cli asset dialogue clerk"
    }
    CustomStringTableItem "{63D92EC5F37B85A8}" {
     Id "LF-Dialogue_Clerk_Greet"
     Target_en_us "Good day. This is the town clerk's desk. What can I do for you?"
     Comment "Dialogue caption, spoken by the clerk NPC (voiced; tools/assets/dialogue/clerk). Changing it needs: tools\\cli asset dialogue clerk"
    }
    CustomStringTableItem "{C74D628DF7469E2D}" {
     Id "LF-Dialogue_Clerk_Idle"
     Target_en_us "You are settled in. The town board on the square shows the rest: the shops, the car lot, the police station and the organization office."
     Comment "Dialogue caption, spoken by the clerk NPC (voiced; tools/assets/dialogue/clerk). Changing it needs: tools\\cli asset dialogue clerk"
    }
    CustomStringTableItem "{CB45DD686F00C171}" {
     Id "LF-Dialogue_Clerk_Register"
     Target_en_us "There. Your name is in the register, and this is your identity card. Keep it on you. The police may ask to see it."
     Comment "Dialogue caption, spoken by the clerk NPC (voiced; tools/assets/dialogue/clerk). Changing it needs: tools\\cli asset dialogue clerk"
    }
    CustomStringTableItem "{50187032E73CA183}" {
     Id "LF-Dialogue_Clerk_SayBye"
     Target_en_us "Goodbye."
     Comment "Dialogue reply the player can pick when talking to the clerk NPC."
    }
    CustomStringTableItem "{A17C3E5D92B84F60}" {
     Id "LF-Dialogue_Clerk_SayFile"
     Target_en_us "What does my file say?"
     Comment "Dialogue reply the player can pick when talking to the clerk NPC: the clerk says where the player stands with the town and the police, in words."
    }
    CustomStringTableItem "{6A2F03A535260E7C}" {
     Id "LF-Dialogue_Clerk_SayLost"
     Target_en_us "I have lost my identity card."
     Comment "Dialogue reply the player can pick when talking to the clerk NPC."
    }
    CustomStringTableItem "{4DDB701E3B9C046A}" {
     Id "LF-Dialogue_Clerk_SayNew"
     Target_en_us "I am new in {town}."
     Comment "Dialogue reply the player can pick when talking to the clerk NPC."
    }
    CustomStringTableItem "{BCE0D0B87758A6FA}" {
     Id "LF-Dialogue_Clerk_SayNext"
     Target_en_us "What should I do now?"
     Comment "Dialogue reply the player can pick when talking to the clerk NPC."
    }
    CustomStringTableItem "{9B92A4F13AC0D017}" {
     Id "LF-Dialogue_Clerk_SayPapers"
     Target_en_us "About my papers."
     Comment "Dialogue reply the player can pick when talking to the clerk NPC."
    }
    CustomStringTableItem "{7F7E08FC92C27753}" {
     Id "LF-Dialogue_Clerk_SaySwitch"
     Target_en_us "I need another of my files. (Change character)"
     Comment "Dialogue reply the player can pick when talking to the clerk NPC."
    }
    CustomStringTableItem "{53AB451CE6742B73}" {
     Id "LF-Dialogue_Clerk_SayTown"
     Target_en_us "Tell me about this {town}."
     Comment "Dialogue reply the player can pick when talking to the clerk NPC."
    }
    CustomStringTableItem "{BBD914A5709DB653}" {
     Id "LF-Dialogue_Clerk_SayWork"
     Target_en_us "Where can I find work?"
     Comment "Dialogue reply the player can pick when talking to the clerk NPC."
    }
    CustomStringTableItem "{D60A962F84ED553E}" {
     Id "LF-Dialogue_Clerk_WorkErrand"
     Target_en_us "Time to earn your first wages. The post office is down the street. Take courier work at the counter, pick up a parcel, carry it to the drop at the east end of the street, and complete the run."
     Comment "Dialogue caption, spoken by the clerk NPC (voiced; tools/assets/dialogue/clerk). Changing it needs: tools\\cli asset dialogue clerk"
    }
    CustomStringTableItem "{3982603FE3CADC9F}" {
     Id "LF-Dialogue_FeeFailed"
     Target_en_us "Not now. Your pockets are full. Your money is still yours."
     Comment "NPC caption: the item could not be handed over (no room); the fee was given back (#4381)."
    }
    CustomStringTableItem "{A6F453B61D373DD7}" {
     Id "LF-Dialogue_FeeHeld"
     Target_en_us "You already have one. Keep your money."
     Comment "NPC caption: the player already carries the item a dialogue fee buys; nothing was taken (#4381)."
    }
    CustomStringTableItem "{6BEE27F424522CB8}" {
     Id "LF-Dialogue_FeePaid"
     Target_en_us "Paid. Here it is."
     Comment "NPC caption after a dialogue fee is paid and the item handed over; a dialogue voices it with a line whose for_key is this key (#4381)."
    }
    CustomStringTableItem "{664E7189E2645BDB}" {
     Id "LF-Dialogue_FeePaidToast"
     Target_en_us "You paid %1 in cash."
     Comment "Toast after paying an NPC's fee in a conversation. %1 is the fee, formatted as money (#4381)."
    }
    CustomStringTableItem "{A156B3598A7D9B80}" {
     Id "LF-Dialogue_FeeShort"
     Target_en_us "That is not enough. Come back with the full amount."
     Comment "NPC caption: the player picked a paid reply but carries less cash than the fee; nothing was taken (#4381)."
    }
    CustomStringTableItem "{6D96E2F833B5BC10}" {
     Id "LF-Dialogue_FeeTitle"
     Target_en_us "Payment"
     Comment "Toast title after paying an NPC's fee in a conversation (#4381)."
    }
    CustomStringTableItem "{D472603DD9705158}" {
     Id "LF-Dialogue_FuelClerk_AboutWhere"
     Target_en_us "This is the only fuel in town, friend. Gas at the pumps, maps and snacks in here."
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{EB24FB16C4F34B2F}" {
     Id "LF-Dialogue_FuelClerk_Beg"
     Target_en_us "Please, I am not a hero. I will not call anyone, I swear it. Just take it."
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{0896B3E1E8535C8F}" {
     Id "LF-Dialogue_FuelClerk_Bye"
     Target_en_us "Drive safe. Mind the potholes on the coast road."
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{F89E614A9ED832B3}" {
     Id "LF-Dialogue_FuelClerk_ChatNews"
     Target_en_us "Army trucks on the coast road again, all night long. Nobody tells us anything. My mother says it is like sixty eight."
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{ADE95A91477ED67E}" {
     Id "LF-Dialogue_FuelClerk_ChatQuiet"
     Target_en_us "Quiet as a church. A tractor at nine, the post van at noon, and now you. The radio says rain by Thursday."
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{CC61CA49E7915664}" {
     Id "LF-Dialogue_FuelClerk_Gone"
     Target_en_us "He is gone? Oh God. Somebody call the police!"
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{7FFE44F7F81E5976}" {
     Id "LF-Dialogue_FuelClerk_Greet"
     Target_en_us "Hello. Pump two sticks, so use pump one. What can I do for you?"
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{74D57E4F126FE30F}" {
     Id "LF-Dialogue_FuelClerk_Paid"
     Target_en_us "There! That is all of it, all of it! Take it and go, please, just go!"
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{E769B1D12183A675}" {
     Id "LF-Dialogue_FuelClerk_Police"
     Target_en_us "Police! In here! He has a gun! Help me!"
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{27029AD6A4C46ABD}" {
     Id "LF-Dialogue_FuelClerk_RobEmpty"
     Target_en_us "It is empty! I swear on my mother, the boss took the cash to the bank. Look, look, there is nothing!"
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{3D26F89BA596C1B8}" {
     Id "LF-Dialogue_FuelClerk_RobOff"
     Target_en_us "Oh God. Oh God, thank you. Just go. Please, just go."
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{5DE47529DF3B5A69}" {
     Id "LF-Dialogue_FuelClerk_RobOpen"
     Target_en_us "Yes! Yes, I am opening it. The key, where is the... here, here it is. Just do not shoot, please, I am doing it!"
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{0FC2714512F080F2}" {
     Id "LF-Dialogue_FuelClerk_RobPanic"
     Target_en_us "W-what? No, no, no. Please, put that away. I have a little girl at home. Please do not shoot me!"
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{30BA5B858FBF23FC}" {
     Id "LF-Dialogue_FuelClerk_SayBye"
     Target_en_us "Goodbye."
     Comment "Dialogue reply the player can pick when talking to the fuel_clerk NPC."
    }
    CustomStringTableItem "{DCA9D412FEC4B04C}" {
     Id "LF-Dialogue_FuelClerk_SayChat"
     Target_en_us "Quiet day?"
     Comment "Dialogue reply the player can pick when talking to the fuel_clerk NPC."
    }
    CustomStringTableItem "{C3A3478410D6A6C7}" {
     Id "LF-Dialogue_FuelClerk_SayDemand"
     Target_en_us "Open the till. Now, or you die behind that counter."
     Comment "Dialogue reply the player can pick when talking to the fuel_clerk NPC."
    }
    CustomStringTableItem "{E0C9B717E8A63B4D}" {
     Id "LF-Dialogue_FuelClerk_SayNews"
     Target_en_us "Anything happening in {town}?"
     Comment "Dialogue reply the player can pick when talking to the fuel_clerk NPC."
    }
    CustomStringTableItem "{47B1E52CE1513EFD}" {
     Id "LF-Dialogue_FuelClerk_SayOff"
     Target_en_us "Forget it. You never saw me."
     Comment "Dialogue reply the player can pick when talking to the fuel_clerk NPC."
    }
    CustomStringTableItem "{78F76EB9DCC3A7F0}" {
     Id "LF-Dialogue_FuelClerk_SayRob"
     Target_en_us "This is a robbery. Hands where I can see them!"
     Comment "Dialogue reply the player can pick when talking to the fuel_clerk NPC."
    }
    CustomStringTableItem "{169564188FA741B2}" {
     Id "LF-Dialogue_FuelClerk_SayWhere"
     Target_en_us "Hey, where are we?"
     Comment "Dialogue reply the player can pick when talking to the fuel_clerk NPC."
    }
    CustomStringTableItem "{7A946164FACF3403}" {
     Id "LF-Dialogue_FuelClerk_Stammer"
     Target_en_us "My hands, I cannot stop my hands. It is coming, it is almost open, please..."
     Comment "Dialogue caption, spoken by the fuel_clerk NPC (voiced; tools/assets/dialogue/fuel_clerk). Changing it needs: tools\\cli asset dialogue fuel_clerk"
    }
    CustomStringTableItem "{D7641543B9FD8567}" {
     Id "LF-Dialogue_Goodbye"
     Target_en_us "Goodbye"
     Comment "Conversation scene: the last numbered row, always 0, that ends the talk with any NPC."
    }
    CustomStringTableItem "{857383630EAB1A5D}" {
     Id "LF-Dialogue_HintChoose"
     Target_en_us "Press the number, or Up, Down and Confirm: answer    0 or Back: goodbye"
     Comment "Conversation scene, small hint under the numbered replies."
    }
    CustomStringTableItem "{B3D8A9678B34F5A7}" {
     Id "LF-Dialogue_HintLine"
     Target_en_us "Confirm: skip    0 or Back: goodbye"
     Comment "Conversation scene, small hint under the caption while the NPC speaks. Confirm and Back are the menu confirm and back keys."
    }
    CustomStringTableItem "{6456C75F481973C0}" {
     Id "LF-Dialogue_Resident_Bye"
     Target_en_us "Take care."
     Comment "Dialogue caption: a resident says goodbye (#2908)."
    }
    CustomStringTableItem "{F3EF6FF492DA0C0C}" {
     Id "LF-Dialogue_Resident_GreetFriendly"
     Target_en_us "Good to see you again. How are you keeping?"
     Comment "Dialogue caption: how a resident greets a character they feel 'friendly' about (#2908)."
    }
    CustomStringTableItem "{99DC983EEF939A94}" {
     Id "LF-Dialogue_Resident_GreetNeutral"
     Target_en_us "Morning. Can I help you with something?"
     Comment "Dialogue caption: how a resident greets a character they feel 'neutral' about (#2908)."
    }
    CustomStringTableItem "{BCA0CA792C68EAAA}" {
     Id "LF-Dialogue_Resident_GreetRefuses"
     Target_en_us "I've nothing to say to you."
     Comment "Dialogue caption: how a resident greets a character they feel 'refuses' about (#2908)."
    }
    CustomStringTableItem "{7E0339705DFF2929}" {
     Id "LF-Dialogue_Resident_GreetRegular"
     Target_en_us "There you are! Come in, come in. It's always good to see you."
     Comment "Dialogue caption: how a resident greets a character they feel 'regular' about (#2908)."
    }
    CustomStringTableItem "{9DC6BC29421DDF04}" {
     Id "LF-Dialogue_Resident_GreetWary"
     Target_en_us "Oh. It's you. Keep it short."
     Comment "Dialogue caption: how a resident greets a character they feel 'wary' about (#2908)."
    }
    CustomStringTableItem "{24A6C708B0B2495C}" {
     Id "LF-Dialogue_Resident_Name"
     Target_en_us "Resident"
     Comment "Dialogue speaker name for a townsperson who speaks through the core resident dialogue."
    }
    CustomStringTableItem "{7C2D782E00522C19}" {
     Id "LF-Dialogue_Resident_SayBye"
     Target_en_us "Goodbye."
     Comment "Dialogue reply: end the talk with a resident (#2908)."
    }
    CustomStringTableItem "{72B8972E2A4208C1}" {
     Id "LF-Dialogue_RoleFence_Bye"
     Target_en_us "We never spoke."
     Comment "Dialogue caption, spoken by the role_fence NPC (text only; tools/assets/dialogue/role_fence). Changing it needs: tools\\cli asset dialogue role_fence"
    }
    CustomStringTableItem "{9CAB529816DBD464}" {
     Id "LF-Dialogue_RoleFence_Greet"
     Target_en_us "You did not see me. Now, what are you after?"
     Comment "Dialogue caption, spoken by the role_fence NPC (text only; tools/assets/dialogue/role_fence). Changing it needs: tools\\cli asset dialogue role_fence"
    }
    CustomStringTableItem "{489F6B2B5E785654}" {
     Id "LF-Dialogue_RoleFence_Rumour"
     Target_en_us "Somebody sold a drill and a charge to the wrong customer. The wrong customer was the {police}."
     Comment "Dialogue caption, spoken by the role_fence NPC (text only; tools/assets/dialogue/role_fence). Changing it needs: tools\\cli asset dialogue role_fence"
    }
    CustomStringTableItem "{C2E59637D5CBE2A6}" {
     Id "LF-Dialogue_RoleFence_SayBrowse"
     Target_en_us "Show me what you've got."
     Comment "Dialogue reply the player can pick when talking to the role_fence NPC."
    }
    CustomStringTableItem "{2F97D1CEAEE77234}" {
     Id "LF-Dialogue_RoleFence_SayBye"
     Target_en_us "Goodbye."
     Comment "Dialogue reply the player can pick when talking to the role_fence NPC."
    }
    CustomStringTableItem "{15418E634F494DAD}" {
     Id "LF-Dialogue_RoleFence_SayMore"
     Target_en_us "One more thing."
     Comment "Dialogue reply the player can pick when talking to the role_fence NPC."
    }
    CustomStringTableItem "{69B6DDE0717502FE}" {
     Id "LF-Dialogue_RoleFence_SayRumour"
     Target_en_us "Heard anything lately?"
     Comment "Dialogue reply the player can pick when talking to the role_fence NPC."
    }
    CustomStringTableItem "{CB888F1B27B62BC7}" {
     Id "LF-Dialogue_RoleFence_SayWhat"
     Target_en_us "What do you do here?"
     Comment "Dialogue reply the player can pick when talking to the role_fence NPC."
    }
    CustomStringTableItem "{24ED4897A132D126}" {
     Id "LF-Dialogue_RoleFence_SayWhere"
     Target_en_us "Can you point me somewhere?"
     Comment "Dialogue reply the player can pick when talking to the role_fence NPC."
    }
    CustomStringTableItem "{2DCEA52CDB49F614}" {
     Id "LF-Dialogue_RoleFence_What"
     Target_en_us "I sell tools to people who do not ask what they are for. Locks are hard and the {police} are harder. Choose the right tool."
     Comment "Dialogue caption, spoken by the role_fence NPC (text only; tools/assets/dialogue/role_fence). Changing it needs: tools\\cli asset dialogue role_fence"
    }
    CustomStringTableItem "{1714310B44EC7DEA}" {
     Id "LF-Dialogue_RoleFence_Where"
     Target_en_us "Ask nobody about me. Keep to the back streets."
     Comment "Dialogue caption, spoken by the role_fence NPC (text only; tools/assets/dialogue/role_fence). Changing it needs: tools\\cli asset dialogue role_fence"
    }
    CustomStringTableItem "{ED9DC30135F01105}" {
     Id "LF-Dialogue_RoleMechanic_Bye"
     Target_en_us "Drive safe."
     Comment "Dialogue caption, spoken by the role_mechanic NPC (text only; tools/assets/dialogue/role_mechanic). Changing it needs: tools\\cli asset dialogue role_mechanic"
    }
    CustomStringTableItem "{8A5E80DD173EF8C2}" {
     Id "LF-Dialogue_RoleMechanic_Greet"
     Target_en_us "Mind the oil. What is wrong with it?"
     Comment "Dialogue caption, spoken by the role_mechanic NPC (text only; tools/assets/dialogue/role_mechanic). Changing it needs: tools\\cli asset dialogue role_mechanic"
    }
    CustomStringTableItem "{4834C033B5A5A764}" {
     Id "LF-Dialogue_RoleMechanic_Rumour"
     Target_en_us "Somebody keeps asking me how to start a car without the key. I tell them to buy the key."
     Comment "Dialogue caption, spoken by the role_mechanic NPC (text only; tools/assets/dialogue/role_mechanic). Changing it needs: tools\\cli asset dialogue role_mechanic"
    }
    CustomStringTableItem "{F6C964C69F6B8F1C}" {
     Id "LF-Dialogue_RoleMechanic_SayBrowse"
     Target_en_us "Show me what you've got."
     Comment "Dialogue reply the player can pick when talking to the role_mechanic NPC."
    }
    CustomStringTableItem "{642F8B4067F562BA}" {
     Id "LF-Dialogue_RoleMechanic_SayBye"
     Target_en_us "Goodbye."
     Comment "Dialogue reply the player can pick when talking to the role_mechanic NPC."
    }
    CustomStringTableItem "{C74F909F4819B632}" {
     Id "LF-Dialogue_RoleMechanic_SayMore"
     Target_en_us "One more thing."
     Comment "Dialogue reply the player can pick when talking to the role_mechanic NPC."
    }
    CustomStringTableItem "{06ED0CB591544621}" {
     Id "LF-Dialogue_RoleMechanic_SayRumour"
     Target_en_us "Heard anything lately?"
     Comment "Dialogue reply the player can pick when talking to the role_mechanic NPC."
    }
    CustomStringTableItem "{A5850A643C048613}" {
     Id "LF-Dialogue_RoleMechanic_SayWhat"
     Target_en_us "What do you do here?"
     Comment "Dialogue reply the player can pick when talking to the role_mechanic NPC."
    }
    CustomStringTableItem "{48F37B5201E8BA51}" {
     Id "LF-Dialogue_RoleMechanic_SayWhere"
     Target_en_us "Can you point me somewhere?"
     Comment "Dialogue reply the player can pick when talking to the role_mechanic NPC."
    }
    CustomStringTableItem "{00D50CF213CD743C}" {
     Id "LF-Dialogue_RoleMechanic_What"
     Target_en_us "I fix cars. A new battery, a repair kit, the right fuel, and it runs. Most breakdowns are a dead battery."
     Comment "Dialogue caption, spoken by the role_mechanic NPC (text only; tools/assets/dialogue/role_mechanic). Changing it needs: tools\\cli asset dialogue role_mechanic"
    }
    CustomStringTableItem "{41AD34A13B8CAFD3}" {
     Id "LF-Dialogue_RoleMechanic_Where"
     Target_en_us "The car lot is on the east side, and the fuel station is down the road. Fill up before you go far."
     Comment "Dialogue caption, spoken by the role_mechanic NPC (text only; tools/assets/dialogue/role_mechanic). Changing it needs: tools\\cli asset dialogue role_mechanic"
    }
    CustomStringTableItem "{A47534F3909B3682}" {
     Id "LF-Dialogue_RoleMedic_Bye"
     Target_en_us "Look after yourself."
     Comment "Dialogue caption, spoken by the role_medic NPC (text only; tools/assets/dialogue/role_medic). Changing it needs: tools\\cli asset dialogue role_medic"
    }
    CustomStringTableItem "{C0790ABB8E8A1045}" {
     Id "LF-Dialogue_RoleMedic_Greet"
     Target_en_us "Come in. Are you hurt?"
     Comment "Dialogue caption, spoken by the role_medic NPC (text only; tools/assets/dialogue/role_medic). Changing it needs: tools\\cli asset dialogue role_medic"
    }
    CustomStringTableItem "{78303B3C326DD13D}" {
     Id "LF-Dialogue_RoleMedic_Rumour"
     Target_en_us "A lot of cuts from the sawmill this month. The men will not wear gloves."
     Comment "Dialogue caption, spoken by the role_medic NPC (text only; tools/assets/dialogue/role_medic). Changing it needs: tools\\cli asset dialogue role_medic"
    }
    CustomStringTableItem "{BEB9F935BABDA793}" {
     Id "LF-Dialogue_RoleMedic_SayBrowse"
     Target_en_us "Show me what you've got."
     Comment "Dialogue reply the player can pick when talking to the role_medic NPC."
    }
    CustomStringTableItem "{76E46E710BAAC893}" {
     Id "LF-Dialogue_RoleMedic_SayBye"
     Target_en_us "Goodbye."
     Comment "Dialogue reply the player can pick when talking to the role_medic NPC."
    }
    CustomStringTableItem "{5A6656089219089F}" {
     Id "LF-Dialogue_RoleMedic_SayMore"
     Target_en_us "One more thing."
     Comment "Dialogue reply the player can pick when talking to the role_medic NPC."
    }
    CustomStringTableItem "{4EDC252C93C5CD44}" {
     Id "LF-Dialogue_RoleMedic_SayRumour"
     Target_en_us "Heard anything lately?"
     Comment "Dialogue reply the player can pick when talking to the role_medic NPC."
    }
    CustomStringTableItem "{D630BB2820B82FFF}" {
     Id "LF-Dialogue_RoleMedic_SayWhat"
     Target_en_us "What do you do here?"
     Comment "Dialogue reply the player can pick when talking to the role_medic NPC."
    }
    CustomStringTableItem "{A9D81D6A9C42641C}" {
     Id "LF-Dialogue_RoleMedic_SayWhere"
     Target_en_us "Can you point me somewhere?"
     Comment "Dialogue reply the player can pick when talking to the role_medic NPC."
    }
    CustomStringTableItem "{52D0C205FD1FC027}" {
     Id "LF-Dialogue_RoleMedic_What"
     Target_en_us "I patch people up. A bandage stops bleeding. Anything worse, come to me before it gets worse."
     Comment "Dialogue caption, spoken by the role_medic NPC (text only; tools/assets/dialogue/role_medic). Changing it needs: tools\\cli asset dialogue role_medic"
    }
    CustomStringTableItem "{4827D5A1883880E8}" {
     Id "LF-Dialogue_RoleMedic_Where"
     Target_en_us "If you need a bandage and I am not here, the general store keeps dressings by the counter."
     Comment "Dialogue caption, spoken by the role_medic NPC (text only; tools/assets/dialogue/role_medic). Changing it needs: tools\\cli asset dialogue role_medic"
    }
    CustomStringTableItem "{4B94B75B08F3D7A1}" {
     Id "LF-Dialogue_RolePolice_Bye"
     Target_en_us "Stay out of trouble."
     Comment "Dialogue caption, spoken by the role_police NPC (text only; tools/assets/dialogue/role_police). Changing it needs: tools\\cli asset dialogue role_police"
    }
    CustomStringTableItem "{070510F9A973D44C}" {
     Id "LF-Dialogue_RolePolice_Greet"
     Target_en_us "Officer on duty. Keep your hands where I can see them, and we will get along."
     Comment "Dialogue caption, spoken by the role_police NPC (text only; tools/assets/dialogue/role_police). Changing it needs: tools\\cli asset dialogue role_police"
    }
    CustomStringTableItem "{03A36F9FF760B4AC}" {
     Id "LF-Dialogue_RolePolice_Rumour"
     Target_en_us "We have had burglaries along the main street. If you saw anything, tell us. People here remember faces."
     Comment "Dialogue caption, spoken by the role_police NPC (text only; tools/assets/dialogue/role_police). Changing it needs: tools\\cli asset dialogue role_police"
    }
    CustomStringTableItem "{B8F1FFC241757861}" {
     Id "LF-Dialogue_RolePolice_SayBrowse"
     Target_en_us "Show me what you've got."
     Comment "Dialogue reply the player can pick when talking to the role_police NPC."
    }
    CustomStringTableItem "{96391F67DB4EFF26}" {
     Id "LF-Dialogue_RolePolice_SayBye"
     Target_en_us "Goodbye."
     Comment "Dialogue reply the player can pick when talking to the role_police NPC."
    }
    CustomStringTableItem "{8036AA8BA603F9C1}" {
     Id "LF-Dialogue_RolePolice_SayMore"
     Target_en_us "One more thing."
     Comment "Dialogue reply the player can pick when talking to the role_police NPC."
    }
    CustomStringTableItem "{FD1045C855D454AF}" {
     Id "LF-Dialogue_RolePolice_SayRumour"
     Target_en_us "Heard anything lately?"
     Comment "Dialogue reply the player can pick when talking to the role_police NPC."
    }
    CustomStringTableItem "{E82C872BB54A7C13}" {
     Id "LF-Dialogue_RolePolice_SayWhat"
     Target_en_us "What do you do here?"
     Comment "Dialogue reply the player can pick when talking to the role_police NPC."
    }
    CustomStringTableItem "{DBC662504B4C29F3}" {
     Id "LF-Dialogue_RolePolice_SayWhere"
     Target_en_us "Can you point me somewhere?"
     Comment "Dialogue reply the player can pick when talking to the role_police NPC."
    }
    CustomStringTableItem "{B829883E7B6C2CD6}" {
     Id "LF-Dialogue_RolePolice_What"
     Target_en_us "I keep the peace and take the reports. Break into a shop or hurt someone and you will see me again."
     Comment "Dialogue caption, spoken by the role_police NPC (text only; tools/assets/dialogue/role_police). Changing it needs: tools\\cli asset dialogue role_police"
    }
    CustomStringTableItem "{B8DF7D1CC370A494}" {
     Id "LF-Dialogue_RolePolice_Where"
     Target_en_us "The station is the building behind me. The {town} board on the square lists everything else."
     Comment "Dialogue caption, spoken by the role_police NPC (text only; tools/assets/dialogue/role_police). Changing it needs: tools\\cli asset dialogue role_police"
    }
    CustomStringTableItem "{215C350813A39835}" {
     Id "LF-Dialogue_RoleStoreClerk_Bye"
     Target_en_us "Take care out there."
     Comment "Dialogue caption, spoken by the role_store_clerk NPC (text only; tools/assets/dialogue/role_store_clerk). Changing it needs: tools\\cli asset dialogue role_store_clerk"
    }
    CustomStringTableItem "{837320B83A023DC6}" {
     Id "LF-Dialogue_RoleStoreClerk_Greet"
     Target_en_us "Morning. Looking for something in particular?"
     Comment "Dialogue caption, spoken by the role_store_clerk NPC (text only; tools/assets/dialogue/role_store_clerk). Changing it needs: tools\\cli asset dialogue role_store_clerk"
    }
    CustomStringTableItem "{0583E2202A9B88AD}" {
     Id "LF-Dialogue_RoleStoreClerk_Rumour"
     Target_en_us "Somebody has been asking whether I keep a safe under the counter. I do not, and I do not keep much cash either."
     Comment "Dialogue caption, spoken by the role_store_clerk NPC (text only; tools/assets/dialogue/role_store_clerk). Changing it needs: tools\\cli asset dialogue role_store_clerk"
    }
    CustomStringTableItem "{D02DE736B1C189F6}" {
     Id "LF-Dialogue_RoleStoreClerk_SayBrowse"
     Target_en_us "Show me what you've got."
     Comment "Dialogue reply the player can pick when talking to the role_store_clerk NPC."
    }
    CustomStringTableItem "{E295C8499B5DC28E}" {
     Id "LF-Dialogue_RoleStoreClerk_SayBye"
     Target_en_us "Goodbye."
     Comment "Dialogue reply the player can pick when talking to the role_store_clerk NPC."
    }
    CustomStringTableItem "{71126CF29DBAF863}" {
     Id "LF-Dialogue_RoleStoreClerk_SayMore"
     Target_en_us "One more thing."
     Comment "Dialogue reply the player can pick when talking to the role_store_clerk NPC."
    }
    CustomStringTableItem "{B557DE56CC98BF98}" {
     Id "LF-Dialogue_RoleStoreClerk_SayRumour"
     Target_en_us "Heard anything lately?"
     Comment "Dialogue reply the player can pick when talking to the role_store_clerk NPC."
    }
    CustomStringTableItem "{DCCDC991624EA96E}" {
     Id "LF-Dialogue_RoleStoreClerk_SayWhat"
     Target_en_us "What do you do here?"
     Comment "Dialogue reply the player can pick when talking to the role_store_clerk NPC."
    }
    CustomStringTableItem "{901913ED0AD3E834}" {
     Id "LF-Dialogue_RoleStoreClerk_SayWhere"
     Target_en_us "Can you point me somewhere?"
     Comment "Dialogue reply the player can pick when talking to the role_store_clerk NPC."
    }
    CustomStringTableItem "{C663B941AD0AF51E}" {
     Id "LF-Dialogue_RoleStoreClerk_What"
     Target_en_us "I run the till and keep the shelves full. Bread, water, apples, dressings. If we do not have it, nobody in {town} does."
     Comment "Dialogue caption, spoken by the role_store_clerk NPC (text only; tools/assets/dialogue/role_store_clerk). Changing it needs: tools\\cli asset dialogue role_store_clerk"
    }
    CustomStringTableItem "{52B9BF00FA3A31A1}" {
     Id "LF-Dialogue_RoleStoreClerk_Where"
     Target_en_us "The market square is south of the main street if you want fruit and vegetables. Cars are sold at the lot on the east side."
     Comment "Dialogue caption, spoken by the role_store_clerk NPC (text only; tools/assets/dialogue/role_store_clerk). Changing it needs: tools\\cli asset dialogue role_store_clerk"
    }
    CustomStringTableItem "{254171EDBB61901B}" {
     Id "LF-Dialogue_RoleTeller_Bye"
     Target_en_us "Good day. Mind your money."
     Comment "Dialogue caption, spoken by the role_teller NPC (text only; tools/assets/dialogue/role_teller). Changing it needs: tools\\cli asset dialogue role_teller"
    }
    CustomStringTableItem "{1E386EA66A79D064}" {
     Id "LF-Dialogue_RoleTeller_Greet"
     Target_en_us "Good day. Welcome to the bank. How can I help?"
     Comment "Dialogue caption, spoken by the role_teller NPC (text only; tools/assets/dialogue/role_teller). Changing it needs: tools\\cli asset dialogue role_teller"
    }
    CustomStringTableItem "{D6055E830E7F5903}" {
     Id "LF-Dialogue_RoleTeller_Rumour"
     Target_en_us "A man with no work and a drill in his bag asked me about the vault. I told him the vault is none of my business."
     Comment "Dialogue caption, spoken by the role_teller NPC (text only; tools/assets/dialogue/role_teller). Changing it needs: tools\\cli asset dialogue role_teller"
    }
    CustomStringTableItem "{C669101DB3ED75DA}" {
     Id "LF-Dialogue_RoleTeller_SayBrowse"
     Target_en_us "Show me what you've got."
     Comment "Dialogue reply the player can pick when talking to the role_teller NPC."
    }
    CustomStringTableItem "{FD8A53E8E314BAB2}" {
     Id "LF-Dialogue_RoleTeller_SayBye"
     Target_en_us "Goodbye."
     Comment "Dialogue reply the player can pick when talking to the role_teller NPC."
    }
    CustomStringTableItem "{F7729E35FD22B45E}" {
     Id "LF-Dialogue_RoleTeller_SayMore"
     Target_en_us "One more thing."
     Comment "Dialogue reply the player can pick when talking to the role_teller NPC."
    }
    CustomStringTableItem "{6229E07D0BE61F10}" {
     Id "LF-Dialogue_RoleTeller_SayRumour"
     Target_en_us "Heard anything lately?"
     Comment "Dialogue reply the player can pick when talking to the role_teller NPC."
    }
    CustomStringTableItem "{466EC34322788915}" {
     Id "LF-Dialogue_RoleTeller_SayWhat"
     Target_en_us "What do you do here?"
     Comment "Dialogue reply the player can pick when talking to the role_teller NPC."
    }
    CustomStringTableItem "{F49C6DECCD91201D}" {
     Id "LF-Dialogue_RoleTeller_SayWhere"
     Target_en_us "Can you point me somewhere?"
     Comment "Dialogue reply the player can pick when talking to the role_teller NPC."
    }
    CustomStringTableItem "{9BDCA80D237262AF}" {
     Id "LF-Dialogue_RoleTeller_What"
     Target_en_us "I keep the counter and the accounts. Cash in your pocket is gone when you die. Money in the bank is not. The cash machine on the wall does deposits and withdrawals."
     Comment "Dialogue caption, spoken by the role_teller NPC (text only; tools/assets/dialogue/role_teller). Changing it needs: tools\\cli asset dialogue role_teller"
    }
    CustomStringTableItem "{1344BEA384FEF57D}" {
     Id "LF-Dialogue_RoleTeller_Where"
     Target_en_us "The post office is right beside us. The {town} board on the square lists the rest, and the {police} station is a short walk up the street."
     Comment "Dialogue caption, spoken by the role_teller NPC (text only; tools/assets/dialogue/role_teller). Changing it needs: tools\\cli asset dialogue role_teller"
    }
    CustomStringTableItem "{760EF9A0B053C005}" {
     Id "LF-Dialogue_RoleTownsfolk_Bye"
     Target_en_us "Take care."
     Comment "Dialogue caption, spoken by the role_townsfolk NPC (text only; tools/assets/dialogue/role_townsfolk). Changing it needs: tools\\cli asset dialogue role_townsfolk"
    }
    CustomStringTableItem "{9D6D3E297FD1E80A}" {
     Id "LF-Dialogue_RoleTownsfolk_Greet"
     Target_en_us "Hello. New in {town}?"
     Comment "Dialogue caption, spoken by the role_townsfolk NPC (text only; tools/assets/dialogue/role_townsfolk). Changing it needs: tools\\cli asset dialogue role_townsfolk"
    }
    CustomStringTableItem "{6996FE7EEA1A16B1}" {
     Id "LF-Dialogue_RoleTownsfolk_Rumour"
     Target_en_us "People say the {police} are watching the main street. Burglars are about. Keep your doors locked."
     Comment "Dialogue caption, spoken by the role_townsfolk NPC (text only; tools/assets/dialogue/role_townsfolk). Changing it needs: tools\\cli asset dialogue role_townsfolk"
    }
    CustomStringTableItem "{EF1E1D862F9B7CD6}" {
     Id "LF-Dialogue_RoleTownsfolk_SayBrowse"
     Target_en_us "Show me what you've got."
     Comment "Dialogue reply the player can pick when talking to the role_townsfolk NPC."
    }
    CustomStringTableItem "{A8908318C66BA656}" {
     Id "LF-Dialogue_RoleTownsfolk_SayBye"
     Target_en_us "Goodbye."
     Comment "Dialogue reply the player can pick when talking to the role_townsfolk NPC."
    }
    CustomStringTableItem "{ECE3B0D4E9E9C443}" {
     Id "LF-Dialogue_RoleTownsfolk_SayMore"
     Target_en_us "One more thing."
     Comment "Dialogue reply the player can pick when talking to the role_townsfolk NPC."
    }
    CustomStringTableItem "{8A9193841DC4B4C7}" {
     Id "LF-Dialogue_RoleTownsfolk_SayRumour"
     Target_en_us "Heard anything lately?"
     Comment "Dialogue reply the player can pick when talking to the role_townsfolk NPC."
    }
    CustomStringTableItem "{A837FF22CF4C203D}" {
     Id "LF-Dialogue_RoleTownsfolk_SayWhat"
     Target_en_us "What do you do here?"
     Comment "Dialogue reply the player can pick when talking to the role_townsfolk NPC."
    }
    CustomStringTableItem "{024D9A2F77FD5231}" {
     Id "LF-Dialogue_RoleTownsfolk_SayWhere"
     Target_en_us "Can you point me somewhere?"
     Comment "Dialogue reply the player can pick when talking to the role_townsfolk NPC."
    }
    CustomStringTableItem "{79E7D7034D374C37}" {
     Id "LF-Dialogue_RoleTownsfolk_What"
     Target_en_us "I live here. I get by, as everyone does. The work is simple and the people are decent."
     Comment "Dialogue caption, spoken by the role_townsfolk NPC (text only; tools/assets/dialogue/role_townsfolk). Changing it needs: tools\\cli asset dialogue role_townsfolk"
    }
    CustomStringTableItem "{2D6C68B37351635D}" {
     Id "LF-Dialogue_RoleTownsfolk_Where"
     Target_en_us "The {town} board on the square shows everything. The post office hires couriers, and the bank is beside it."
     Comment "Dialogue caption, spoken by the role_townsfolk NPC (text only; tools/assets/dialogue/role_townsfolk). Changing it needs: tools\\cli asset dialogue role_townsfolk"
    }
    CustomStringTableItem "{0480851C614F500C}" {
     Id "LF-Dialogue_RoleTrader_Bye"
     Target_en_us "Come back when you have more."
     Comment "Dialogue caption, spoken by the role_trader NPC (text only; tools/assets/dialogue/role_trader). Changing it needs: tools\\cli asset dialogue role_trader"
    }
    CustomStringTableItem "{044A09B67AE8403E}" {
     Id "LF-Dialogue_RoleTrader_Greet"
     Target_en_us "Fresh today. Have a look."
     Comment "Dialogue caption, spoken by the role_trader NPC (text only; tools/assets/dialogue/role_trader). Changing it needs: tools\\cli asset dialogue role_trader"
    }
    CustomStringTableItem "{85382A3BA6F52559}" {
     Id "LF-Dialogue_RoleTrader_Rumour"
     Target_en_us "The price of apples went up this week. Somebody is hoarding them, I think."
     Comment "Dialogue caption, spoken by the role_trader NPC (text only; tools/assets/dialogue/role_trader). Changing it needs: tools\\cli asset dialogue role_trader"
    }
    CustomStringTableItem "{B9F1EEBA03AEE5C2}" {
     Id "LF-Dialogue_RoleTrader_SayBrowse"
     Target_en_us "Show me what you've got."
     Comment "Dialogue reply the player can pick when talking to the role_trader NPC."
    }
    CustomStringTableItem "{06AFB982644BACC5}" {
     Id "LF-Dialogue_RoleTrader_SayBye"
     Target_en_us "Goodbye."
     Comment "Dialogue reply the player can pick when talking to the role_trader NPC."
    }
    CustomStringTableItem "{CF810FCD0C599BCE}" {
     Id "LF-Dialogue_RoleTrader_SayMore"
     Target_en_us "One more thing."
     Comment "Dialogue reply the player can pick when talking to the role_trader NPC."
    }
    CustomStringTableItem "{35ECA074421F23AF}" {
     Id "LF-Dialogue_RoleTrader_SayRumour"
     Target_en_us "Heard anything lately?"
     Comment "Dialogue reply the player can pick when talking to the role_trader NPC."
    }
    CustomStringTableItem "{348716388B238F15}" {
     Id "LF-Dialogue_RoleTrader_SayWhat"
     Target_en_us "What do you do here?"
     Comment "Dialogue reply the player can pick when talking to the role_trader NPC."
    }
    CustomStringTableItem "{161CFACD316FD080}" {
     Id "LF-Dialogue_RoleTrader_SayWhere"
     Target_en_us "Can you point me somewhere?"
     Comment "Dialogue reply the player can pick when talking to the role_trader NPC."
    }
    CustomStringTableItem "{4FB85CE47EB1A778}" {
     Id "LF-Dialogue_RoleTrader_What"
     Target_en_us "I buy and sell at the stall. Bring me what you grow or make and I will give you a fair price."
     Comment "Dialogue caption, spoken by the role_trader NPC (text only; tools/assets/dialogue/role_trader). Changing it needs: tools\\cli asset dialogue role_trader"
    }
    CustomStringTableItem "{A975EEDFA78026D4}" {
     Id "LF-Dialogue_RoleTrader_Where"
     Target_en_us "The farm is out past the greenhouse. For anything else, the {town} board on the square will tell you."
     Comment "Dialogue caption, spoken by the role_trader NPC (text only; tools/assets/dialogue/role_trader). Changing it needs: tools\\cli asset dialogue role_trader"
    }
    CustomStringTableItem "{BC6FD589CFCCD2F2}" {
     Id "LF-Dialogue_Shopkeeper_Bye"
     Target_en_us "Good day to you."
     Comment "Dialogue caption, spoken by the shopkeeper NPC (text only; tools/assets/dialogue/shopkeeper). Changing it needs: tools\\cli asset dialogue shopkeeper"
    }
    CustomStringTableItem "{BEEB7BF6DB95C0F4}" {
     Id "LF-Dialogue_Shopkeeper_Greet"
     Target_en_us "Good day. Take what you need from the shelves and pay at the till."
     Comment "Dialogue caption, spoken by the shopkeeper NPC (text only; tools/assets/dialogue/shopkeeper). Changing it needs: tools\\cli asset dialogue shopkeeper"
    }
    CustomStringTableItem "{A0B028391EADDA2B}" {
     Id "LF-Dialogue_Shopkeeper_HowToPay"
     Target_en_us "Pick the goods up yourself, then pay at any till before you leave. You get a receipt. Walk out without paying and the alarm goes off."
     Comment "Dialogue caption, spoken by the shopkeeper NPC (text only; tools/assets/dialogue/shopkeeper). Changing it needs: tools\\cli asset dialogue shopkeeper"
    }
    CustomStringTableItem "{D49BC0984F3395E4}" {
     Id "LF-Dialogue_Shopkeeper_SayBye"
     Target_en_us "Goodbye."
     Comment "Dialogue reply the player can pick when talking to the shopkeeper NPC."
    }
    CustomStringTableItem "{C3B22B8F511083DF}" {
     Id "LF-Dialogue_Shopkeeper_SayHeard"
     Target_en_us "Heard anything?"
     Comment "Dialogue reply the player can pick when talking to the shopkeeper NPC."
    }
    CustomStringTableItem "{16D8E0E89EE9DE1A}" {
     Id "LF-Dialogue_Shopkeeper_SayPay"
     Target_en_us "How do I pay?"
     Comment "Dialogue reply the player can pick when talking to the shopkeeper NPC."
    }
    CustomStringTableItem "{88EC22D25AF36768}" {
     Id "LF-Dialogue_Shopkeeper_SayStock"
     Target_en_us "What do you sell?"
     Comment "Dialogue reply the player can pick when talking to the shopkeeper NPC."
    }
    CustomStringTableItem "{E91C527F0E5B5487}" {
     Id "LF-Dialogue_Shopkeeper_Stock"
     Target_en_us "Food and household goods on the shelves, drinks in the chilled counter. What runs out comes back from the stockroom every few minutes."
     Comment "Dialogue caption, spoken by the shopkeeper NPC (text only; tools/assets/dialogue/shopkeeper). Changing it needs: tools\\cli asset dialogue shopkeeper"
    }
    CustomStringTableItem "{90674ED1955BD20D}" {
     Id "LF-Dialogue_Shopkeeper_Thief"
     Target_en_us "Stop! You did not pay for that! Come back here!"
     Comment "Dialogue caption, spoken by the shopkeeper NPC (text only; tools/assets/dialogue/shopkeeper). Changing it needs: tools\\cli asset dialogue shopkeeper"
    }
    CustomStringTableItem "{123EFB6232FAF7F2}" {
     Id "LF-Dispatch_BoardHeading"
     Target_en_us "DISPATCH LOG - newest first"
     Comment "Wanted board, police view: heading above the recent dispatch calls."
    }
    CustomStringTableItem "{19D578215BBBA07B}" {
     Id "LF-Dispatch_BoardLine"
     Target_en_us "%1  %2 at %3: %4"
     Comment "Wanted board dispatch line. %1 = clock time HH:MM, %2 = offence, %3 = place, %4 = what happened to the call."
    }
    CustomStringTableItem "{3C88CC2A8BE5A6A5}" {
     Id "LF-Dispatch_BoardLoud"
     Target_en_us "nobody on duty, loud alarm"
     Comment "Dispatch outcome when no officer was on duty: the loud alarm sounded at the site instead."
    }
    CustomStringTableItem "{5D059C7A2F962F91}" {
     Id "LF-Dispatch_BoardRadioed"
     Target_en_us "%1 on duty, radioed"
     Comment "Dispatch outcome: %1 = number of officers on duty who got the radio call."
    }
    CustomStringTableItem "{80FDC5082308648B}" {
     Id "LF-Dispatch_BoardTagged"
     Target_en_us "%1 (%2)"
     Comment "Dispatch board line with a tag after it (#4585). %1 = the dispatch line, %2 = the tag, e.g. low priority."
    }
    CustomStringTableItem "{F03D5F8438D95A26}" {
     Id "LF-Dispatch_Radio"
     Target_en_us "All units: %1 at %2. Suspect: %3."
     Comment "Radio dispatch line. %1 = offence, %2 = place, %3 = description (what they wore, on foot or driving). Never a name."
    }
    CustomStringTableItem "{B4664D355C98920F}" {
     Id "LF-Dispatch_RadioCall"
     Target_en_us "All units: %1 at %2. Reported by phone."
     Comment "Radio dispatch line for a call phoned in by a witness. %1 = what happened, %2 = place. No suspect."
    }
    CustomStringTableItem "{CBEE6929FA9220E9}" {
     Id "LF-Dispatch_Read"
     Target_en_us "Read the dispatch board"
     Comment "Action on a service dispatch board (medic, taxi)."
    }
    CustomStringTableItem "{3C58DD873A36D166}" {
     Id "LF-Dispatch_ServiceClosed"
     Target_en_us "This board is for on-duty staff of the service."
     Comment "Service dispatch board card shown to anyone the service would not reach."
    }
    CustomStringTableItem "{A110BD40681B9DE6}" {
     Id "LF-Dispatch_ServiceLine"
     Target_en_us "%1  %2 at %3"
     Comment "Service dispatch board line. %1 = clock time HH:MM, %2 = what was reported, %3 = place."
    }
    CustomStringTableItem "{C14AC728DB5CB79F}" {
     Id "LF-Dispatch_ServiceLineTaken"
     Target_en_us "%1  %2 at %3 (taken)"
     Comment "Service dispatch board line for a call a unit has taken. %1 = time, %2 = what, %3 = place."
    }
    CustomStringTableItem "{DD4BB860497316E7}" {
     Id "LF-Dispatch_ServiceNone"
     Target_en_us "No calls waiting."
     Comment "Service dispatch board card when no call is open."
    }
    CustomStringTableItem "{40DB3ECCE367BACE}" {
     Id "LF-Dispatch_ServiceTitle"
     Target_en_us "%1 - dispatch"
     Comment "Title of a service dispatch board card. %1 = the service name."
    }
    CustomStringTableItem "{B608F834846FAF73}" {
     Id "LF-Dispatch_Take"
     Target_en_us "Take the call"
     Comment "Action on a service dispatch board: take the oldest open call (#251)."
    }
    CustomStringTableItem "{62139B83F8E9AC01}" {
     Id "LF-Dispatch_TakeDone"
     Target_en_us "Call taken. Head to the place on the board."
     Comment "Toast to the responder after taking a call."
    }
    CustomStringTableItem "{E26C8E0B2B297718}" {
     Id "LF-Dispatch_TakeGone"
     Target_en_us "Another unit already took that call."
     Comment "Toast when the call has all the takers it allows."
    }
    CustomStringTableItem "{B8AB7F7CB0A45CFA}" {
     Id "LF-Dispatch_TakeNone"
     Target_en_us "No call waiting."
     Comment "Toast when the board has no call to take."
    }
    CustomStringTableItem "{7D2865141D4A525F}" {
     Id "LF-Dispatch_TakeYours"
     Target_en_us "You already took every call waiting."
     Comment "Toast when the responder already took the open calls."
    }
    CustomStringTableItem "{E9CE669FFF4D0206}" {
     Id "LF-Dispatch_TestDesc"
     Target_en_us "Playtest only: place a call to an on-duty service without a second player."
     Comment "Description of the dispatch console test-call actions."
    }
    CustomStringTableItem "{803BB29C950EAC01}" {
     Id "LF-Dispatch_TestMedic"
     Target_en_us "Test medic call"
     Comment "Dispatch console action (debug game mode only): place a test medic call from the fuel station."
    }
    CustomStringTableItem "{36AF55270A37E7B0}" {
     Id "LF-Dispatch_TestOff"
     Target_en_us "Test calls are off in this game mode."
     Comment "Toast when the dispatch console test call is used where m_bDebugTestDispatch is off."
    }
    CustomStringTableItem "{9BE7A1D673564E1C}" {
     Id "LF-Dispatch_TestPolice"
     Target_en_us "Test police call"
     Comment "Dispatch console action (debug game mode only): place a test police call from the fuel station till."
    }
    CustomStringTableItem "{51294940A40506CF}" {
     Id "LF-Dispatch_TestSent"
     Target_en_us "Call placed. Take it at the dispatch board."
     Comment "Toast after a test call went out to an on-duty service."
    }
    CustomStringTableItem "{05CAE71B3EBFC584}" {
     Id "LF-Dispatch_TestTitle"
     Target_en_us "Test call"
     Comment "Toast title for the dispatch console test call."
    }
    CustomStringTableItem "{69BB0FD8421E7F07}" {
     Id "LF-Dispatch_Title"
     Target_en_us "Radio - dispatch"
     Comment "Title of the radio dispatch an on-duty officer gets when a crime is reported (M1-20)."
    }
    CustomStringTableItem "{F069A4CD98ADF898}" {
     Id "LF-Dispatch_UnknownPlace"
     Target_en_us "an unknown place"
     Comment "Dispatch place when the crime report has no place."
    }
    CustomStringTableItem "{7E260D671F665668}" {
     Id "LF-Doctor_Coat_Name"
     Target_en_us "Doctor's white coat"
     Comment "Role uniform item name (tools/assets/police-*, doctor-coat)"
    }
    CustomStringTableItem "{3D8A61F0B24C9E75}" {
     Id "LF-DollMask_Description"
     Target_en_us "A molded white plastic doll mask with a painted smile and rosy cheeks, the kind from a toy shop. Anyone wearing it can't be recognized by their face."
     Comment "Headgear item description (tools/assets/doll-mask)"
    }
    CustomStringTableItem "{9E4C27B1A0583DF6}" {
     Id "LF-DollMask_Name"
     Target_en_us "Smiling Doll Mask"
     Comment "Headgear item name (tools/assets/doll-mask)"
    }
    CustomStringTableItem "{59E6B892B42DD6DB}" {
     Id "LF-DownedCall_Summary"
     Target_en_us "Person down, bleeding out"
     Comment "Medic call summary (board row, pager card) placed automatically when a player goes down (#5152)."
    }
    CustomStringTableItem "{7D2A91C4E5B3F609}" {
     Id "LF-DownedHud_Ready"
     Target_en_us "Bleeding out: %1. Hold X to give up, or wait for help."
     Comment "HUD line of a downed player once give-up is open (#5155). %1 is the bleed-out time left (m:ss)."
    }
    CustomStringTableItem "{7D2A91C4E5B3F608}" {
     Id "LF-DownedHud_Wait"
     Target_en_us "Bleeding out: %1. You can give up in %2."
     Comment "HUD line of a downed player before give-up opens (#5155). %1 is the bleed-out time left (m:ss), %2 the wait before give-up (m:ss)."
    }
    CustomStringTableItem "{B20154B0D085697C}" {
     Id "LF-Duty_AlreadyOn"
     Target_en_us "You are already on duty."
     Comment "Duty desk refusal: a second clock-in."
    }
    CustomStringTableItem "{073A8C6E2A477B10}" {
     Id "LF-Duty_ClockDesc"
     Target_en_us "Punch your card at the station duty desk."
     Comment "Description of the duty desk action."
    }
    CustomStringTableItem "{0A0AF64EEE5BACBB}" {
     Id "LF-Duty_ClockIn"
     Target_en_us "Clock in"
     Comment "Duty desk action: punch the card and go on duty as police. Swaps to the police inventory."
    }
    CustomStringTableItem "{E4D4E1EE39E260F2}" {
     Id "LF-Duty_ClockOut"
     Target_en_us "Clock out"
     Comment "Duty desk action: punch the card, go off duty, collect duty pay, get the civilian inventory back."
    }
    CustomStringTableItem "{62E1A14A9FCEE005}" {
     Id "LF-Duty_ClockedIn"
     Target_en_us "You are now on duty. Your own clothes, cash and things wait in the duty locker until you clock out."
     Comment "Toast at the duty desk on clock-in: the duty locker keeps the civilian inventory, cash included, and gives it back at clock-out"
    }
    CustomStringTableItem "{D7EB4DB6446F954F}" {
     Id "LF-Duty_LockerCashReturned"
     Target_en_us "Your {currency:sym}%1 cash is back from your duty locker."
     Comment "Toast at clock-out when the civilian inventory handed back held cash (#5148); %1 is the amount. Shown apart from the duty pay toast."
    }
    CustomStringTableItem "{AB08E8AF6A8954AD}" {
     Id "LF-Duty_NoAccount"
     Target_en_us "Register your character first."
     Comment "Duty desk refusal: the player has no registered character."
    }
    CustomStringTableItem "{F9746F64FC7DF70C}" {
     Id "LF-Duty_NotOn"
     Target_en_us "You are not on duty."
     Comment "Duty desk refusal: clock-out while off duty."
    }
    CustomStringTableItem "{F8D832E3CE9C3E7A}" {
     Id "LF-Duty_NotOnRoster"
     Target_en_us "Only officers on the {police} roster can clock in. Use Apply at this desk to join the {police}."
     Comment "Duty desk refusal: the player is not whitelisted for police."
    }
    CustomStringTableItem "{342841806FB0631D}" {
     Id "LF-Duty_OffDuty"
     Target_en_us "off duty"
     Comment "Inserted into the duty status sentence (EL-Duty_Status_Change, You are now %1)"
    }
    CustomStringTableItem "{745AC868616D92EC}" {
     Id "LF-Duty_OnDuty"
     Target_en_us "on duty"
     Comment "Inserted into the duty status sentence (EL-Duty_Status_Change, You are now %1)"
    }
    CustomStringTableItem "{BDF3D838FB6219D0}" {
     Id "LF-Duty_TrialExpired"
     Target_en_us "Your trial as an officer has ended. You have been clocked out."
     Comment "Toast to an on-duty officer whose trial ran out: the server clocked them out and removed them from the police roster (#2416)."
    }
    CustomStringTableItem "{10850C37306E78EB}" {
     Id "LF-Duty_WrongDesk"
     Target_en_us "You are on duty for another service. Clock out at your own desk first."
     Comment "Duty desk refusal: the player is clocked in as police or medic and used the other desk."
    }
    CustomStringTableItem "{623C4879FE9C4E51}" {
     Id "LF-Emote_MenuTitle"
     Target_en_us "Emotes"
     Comment "Title of the emote radial (hold F1) (#982)."
    }
    CustomStringTableItem "{946FA59E4FF609B8}" {
     Id "LF-Emote_Restrained"
     Target_en_us "Not while restrained"
     Comment "Greyed-out reason on an emote or on Sit while the body is cuffed or stunned (#982)."
    }
    CustomStringTableItem "{43F735E577FB3E9D}" {
     Id "LF-Escort_Action"
     Target_en_us "Escort"
     Comment "Officer action on a cuffed suspect: walk them along."
    }
    CustomStringTableItem "{2AB2FE3B951F79E3}" {
     Id "LF-Escort_HeldByOther"
     Target_en_us "Another officer already has them."
     Comment "Toast to an officer whose escort grab was refused: the suspect is held by a different officer."
    }
    CustomStringTableItem "{5B1F9A3E7C2D4086}" {
     Id "LF-EventMap_Place"
     Target_en_us "Event"
     Comment "Map mark and area label: where a story event is running, shown to everyone until it ends. A pack may override the Id with a place or event name."
    }
    CustomStringTableItem "{7D3E91A4C26B58F1}" {
     Id "LF-EvidenceConvoy_Title"
     Target_en_us "Evidence transfer"
     Comment "Title of the dispatch toast the on-duty police get when the evidence convoy leaves."
    }
    CustomStringTableItem "{3B6A0F58E1D47C92}" {
     Id "LF-EvidenceConvoy_Underway"
     Target_en_us "Evidence transfer under way."
     Comment "Dispatch toast to on-duty police: the evidence convoy has left the station with the seized goods."
    }
    CustomStringTableItem "{4587AF3655D18A21}" {
     Id "LF-Evidence_Bag_Action"
     Target_en_us "Bag as evidence"
     Comment "Action: an officer bags scene evidence."
    }
    CustomStringTableItem "{8F467BFA9C1D7950}" {
     Id "LF-Evidence_Bag_Desc"
     Target_en_us "Sealed at a crime scene. It records where and when the piece was found and which report it belongs to."
     Comment "Item description of the evidence bag."
    }
    CustomStringTableItem "{08D26A0306C3269E}" {
     Id "LF-Evidence_Bag_Name"
     Target_en_us "Evidence bag"
     Comment "Item name of the evidence bag."
    }
    CustomStringTableItem "{A47AEE52195635C1}" {
     Id "LF-Evidence_Bagged"
     Target_en_us "Bagged. The bag lists the place, the time and the report."
     Comment "Toast: an officer bagged a piece of evidence."
    }
    CustomStringTableItem "{B3D1A8AC054C63B7}" {
     Id "LF-Evidence_Casing_Desc"
     Target_en_us "A spent casing from a shot fired at a crime scene."
     Comment "Item description of the casing."
    }
    CustomStringTableItem "{2B763219ECA24CB8}" {
     Id "LF-Evidence_Casing_Name"
     Target_en_us "Spent casing"
     Comment "Item name of a casing left at a crime scene."
    }
    CustomStringTableItem "{12E9D2C263E2307D}" {
     Id "LF-Evidence_Marker_Desc"
     Target_en_us "Marks an item dropped at a crime scene."
     Comment "Description of the evidence marker."
    }
    CustomStringTableItem "{13CC981550C3918A}" {
     Id "LF-Evidence_Marker_Name"
     Target_en_us "Evidence marker"
     Comment "Name of the marker beside a dropped item at a crime scene."
    }
    CustomStringTableItem "{FCFCCDFEF32ED302}" {
     Id "LF-Evidence_OnDutyOnly"
     Target_en_us "Only on-duty {police} bag evidence."
     Comment "Refusal: bagging needs an on-duty officer."
    }
    CustomStringTableItem "{6A6002C800B9F27D}" {
     Id "LF-Evidence_Read"
     Target_en_us "Report %1"
     Comment "Toast on reading an evidence bag; %1 is the report line."
    }
    CustomStringTableItem "{A310F950EFD60150}" {
     Id "LF-Evidence_Read_Action"
     Target_en_us "Read evidence bag"
     Comment "Action: read an evidence bag."
    }
    CustomStringTableItem "{E7C31C63E21652CF}" {
     Id "LF-Evidence_Title"
     Target_en_us "Evidence"
     Comment "Toast title for scene evidence."
    }
    CustomStringTableItem "{6D4D3AC06971B7AB}" {
     Id "LF-ExportBoard_Label"
     Target_en_us "%1 x %2 to %3"
     Comment "Export board line, left: %1 count, %2 goods, %3 destination."
    }
    CustomStringTableItem "{549877E1A15E5C3B}" {
     Id "LF-ExportBoard_Next"
     Target_en_us "Reach Delivery level 10, then take one order here."
     Comment "Sign catalog 'next' text of the export order board."
    }
    CustomStringTableItem "{4D2F0366855B9C5F}" {
     Id "LF-ExportBoard_None"
     Target_en_us "No export orders open"
     Comment "Export board: the line printed when the book is empty."
    }
    CustomStringTableItem "{B8ED2D4329B98696}" {
     Id "LF-ExportBoard_Take"
     Target_en_us "Take the order: %1"
     Comment "Action on the export order board. %1 = the line, e.g. '20 x iron bars to the steamer'."
    }
    CustomStringTableItem "{97ECFD4AF3EADE58}" {
     Id "LF-ExportBoard_Taken"
     Target_en_us "taken"
     Comment "Export board: shown for a taken order when the reader does not know the hauler's name."
    }
    CustomStringTableItem "{4053BE6CB7189143}" {
     Id "LF-ExportBoard_Title"
     Target_en_us "Export orders"
     Comment "Sign title of the dock export order board."
    }
    CustomStringTableItem "{F45CF14B1EA62E15}" {
     Id "LF-ExportBoard_Value"
     Target_en_us "{currency:sym}%1 by %2"
     Comment "Export board line, right, an open order: %1 pay in dollars, %2 deadline (24 h clock)."
    }
    CustomStringTableItem "{E694C546DCD90482}" {
     Id "LF-ExportBoard_ValueTaken"
     Target_en_us "{currency:sym}%1 by %2 - %3"
     Comment "Export board line, right, a taken order: %1 pay, %2 deadline, %3 the hauler's name if known else 'taken'."
    }
    CustomStringTableItem "{0CD0D32BF7C637E8}" {
     Id "LF-ExportBoard_What"
     Target_en_us "Orders from the steamer and the freighters: bring the goods to the crane before the deadline."
     Comment "Sign catalog 'what' text of the export order board (the face prints the live orders)."
    }
    CustomStringTableItem "{E4BA35B4EE3B794F}" {
     Id "LF-ExportCrane_HandIn"
     Target_en_us "Hand in the export order"
     Comment "Action row at the dock crane spot: hand in the goods of the export order you took."
    }
    CustomStringTableItem "{CE33AF47A541C472}" {
     Id "LF-ExportCrane_Paid"
     Target_en_us "Order handed in. The shipper paid you {currency:sym}%1."
     Comment "Toast after a full export order hand-in at the crane. %1 = the cash paid."
    }
    CustomStringTableItem "{033372B427C8F462}" {
     Id "LF-ExportOrder_RefuseGone"
     Target_en_us "That order is no longer on the board."
     Comment "Export order board: the order closed or was never open."
    }
    CustomStringTableItem "{8651760D8ABA0ABB}" {
     Id "LF-ExportOrder_RefuseHoldsOne"
     Target_en_us "You already have an export order. Hand it in first."
     Comment "Export order board: one order per hauler."
    }
    CustomStringTableItem "{75449B377FCD4EF0}" {
     Id "LF-ExportOrder_RefuseLevel"
     Target_en_us "Export orders are for haulers at level 10 or higher. Keep hauling."
     Comment "Export order board: a hauler under the level gate tries to take an order."
    }
    CustomStringTableItem "{827665D19E29D2A6}" {
     Id "LF-ExportOrder_RefuseNoneHeld"
     Target_en_us "You have no export order to hand in."
     Comment "Export order crane: the player holds no order."
    }
    CustomStringTableItem "{333B1F31AD7A4561}" {
     Id "LF-ExportOrder_RefuseShort"
     Target_en_us "That is not the full order. Bring every unit, then hand it in."
     Comment "Export order crane: fewer goods than the order asks for; nothing is taken."
    }
    CustomStringTableItem "{6A1F0C3D9B7E4285}" {
     Id "LF-ExportOrder_RefuseShortCount"
     Target_en_us "That is not the full order: %1 more unit(s) needed. Bring every unit, then hand it in."
     Comment "Export order crane: fewer goods than the order asks for; nothing is taken. %1 = how many units are still missing."
    }
    CustomStringTableItem "{1BA87EB81FC07ECF}" {
     Id "LF-ExportOrder_RefuseTaken"
     Target_en_us "Another hauler already took that order."
     Comment "Export order board: someone else holds the order."
    }
    CustomStringTableItem "{74ADDCEE01ADC72A}" {
     Id "LF-ExportOrder_Taken"
     Target_en_us "Order taken. Hand it in at the dock crane by %1."
     Comment "Toast after taking an export order. %1 = the deadline (24 h clock)."
    }
    CustomStringTableItem "{E6E02AC329CA9010}" {
     Id "LF-Farm_CabbageSeeds"
     Target_en_us "Cabbage seeds"
     Comment "Item name of the cabbage seed packet."
    }
    CustomStringTableItem "{4BBDEB5764438BAB}" {
     Id "LF-Farm_CabbageSeedsDesc"
     Target_en_us "A paper packet of cabbage seeds. Plant it in an empty dirt plot at the farm."
     Comment "Item description of the cabbage seed packet."
    }
    CustomStringTableItem "{EF0CF71534FF8BB4}" {
     Id "LF-Farm_Growing"
     Target_en_us "Growing: %1"
     Comment "Prompt on a farm plot that is not ripe yet. %1 is the stage name (sprouts, half grown)."
    }
    CustomStringTableItem "{EBE3EB8BC96D9CCE}" {
     Id "LF-Farm_Harvest"
     Target_en_us "Harvest %1"
     Comment "Prompt on a ripe farm plot. %1 is the crop item name, e.g. Harvest Cabbage."
    }
    CustomStringTableItem "{AAC69C040DF1C8E4}" {
     Id "LF-Farm_NeedSeeds"
     Target_en_us "Needs a seed packet"
     Comment "Why an empty plot cannot be planted: the player carries no seed packet."
    }
    CustomStringTableItem "{540BF19A61EAB5E6}" {
     Id "LF-Farm_NotEmpty"
     Target_en_us "Something already grows here"
     Comment "Refusal toast: the plot is already planted."
    }
    CustomStringTableItem "{7BCEF6A37309AB1F}" {
     Id "LF-Farm_NotRipe"
     Target_en_us "Not ripe yet"
     Comment "Why a growing plot cannot be harvested."
    }
    CustomStringTableItem "{35C14DE34CACED8B}" {
     Id "LF-Farm_Plant"
     Target_en_us "Plant %1"
     Comment "Prompt on an empty farm plot. %1 is the seed packet name, e.g. Plant Cabbage seeds."
    }
    CustomStringTableItem "{67049824C323938A}" {
     Id "LF-Farm_PocketsFull"
     Target_en_us "No room for the harvest"
     Comment "Refusal toast: the harvest could not be given to the player."
    }
    CustomStringTableItem "{8ABD1690FC2C6972}" {
     Id "LF-Farm_Stage_Growing"
     Target_en_us "half grown"
     Comment "Growth stage name shown in the plot prompt: halfway."
    }
    CustomStringTableItem "{760335DE2D0C4EC2}" {
     Id "LF-Farm_Stage_Ripe"
     Target_en_us "ripe"
     Comment "Growth stage name: ready to harvest."
    }
    CustomStringTableItem "{E66891B20DA6676E}" {
     Id "LF-Farm_Stage_Sprout"
     Target_en_us "sprouts"
     Comment "Growth stage name shown in the plot prompt: just planted."
    }
    CustomStringTableItem "{FFEB328217318791}" {
     Id "LF-Farm_TakeSeeds"
     Target_en_us "Take %1"
     Comment "Prompt on the seed crate at the farm. %1 is the item name, e.g. Take Cabbage seeds."
    }
    CustomStringTableItem "{9EB58969BEDD9423}" {
     Id "LF-Farm_Title"
     Target_en_us "Farm"
     Comment "Toast title for a farm plot refusal."
    }
    CustomStringTableItem "{80208C45C60900DC}" {
     Id "LF-Farm_TooFar"
     Target_en_us "Stand next to the plot"
     Comment "Refusal toast: the player is too far from the plot."
    }
    CustomStringTableItem "{B0445D47AEAB9C63}" {
     Id "LF-Fishing_AlreadyOut"
     Target_en_us "Your line is already out."
     Comment "Toast: a second cast while one is out (#2930)."
    }
    CustomStringTableItem "{81924E1B885918BD}" {
     Id "LF-Fishing_Bite"
     Target_en_us "Strike!"
     Comment "Toast prompt: the fish bit, strike now (#2930)."
    }
    CustomStringTableItem "{1946E4A004ECDA03}" {
     Id "LF-Fishing_Cannot"
     Target_en_us "You cannot fish right now."
     Comment "Toast: any other reason a cast is refused (#2930)."
    }
    CustomStringTableItem "{8CD441A093B5E4D8}" {
     Id "LF-Fishing_CastOut"
     Target_en_us "Line out. Wait for the float to dip."
     Comment "Toast: the cast went out (#2930)."
    }
    CustomStringTableItem "{7DAB65CD9C11C4D6}" {
     Id "LF-Fishing_Caught"
     Target_en_us "You landed a fish."
     Comment "Toast: a strike inside the window gave a fish (#2930)."
    }
    CustomStringTableItem "{5B8680B38E02BCD8}" {
     Id "LF-Fishing_Empty"
     Target_en_us "Nothing was on the line."
     Comment "Toast: the strike landed but no fish could be given (#2930)."
    }
    CustomStringTableItem "{5790E82CE5251627}" {
     Id "LF-Fishing_Level"
     Target_en_us "You need more fishing experience for this rod."
     Comment "Toast: the rod needs a higher FISHER level (#2930)."
    }
    CustomStringTableItem "{32889A24787436BC}" {
     Id "LF-Fishing_Missed"
     Target_en_us "Too late. The bait is gone."
     Comment "Toast: the strike came after the window, or never (#2930)."
    }
    CustomStringTableItem "{C5BD575D50A5AAF2}" {
     Id "LF-Fishing_NoBait"
     Target_en_us "You need bait. Buy a can at the chandler."
     Comment "Toast: a cast with no bait in the inventory (#2930)."
    }
    CustomStringTableItem "{7D3E91A0C54B2F68}" {
     Id "LF-Fishing_NoLicence"
     Target_en_us "You need a fishing license to cast. %1"
     Comment "Toast: a cast when the fishing config names a licence the angler lacks (#3449); %1 says where to get it and the fee (#3150)."
    }
    CustomStringTableItem "{10A11C7FDC837A35}" {
     Id "LF-Fishing_NoRod"
     Target_en_us "Take the fishing rod into your hands first."
     Comment "Toast: a cast without the rod in hand (#2930)."
    }
    CustomStringTableItem "{B9929BF760F3D4E0}" {
     Id "LF-Fishing_NotWater"
     Target_en_us "Cast onto the water."
     Comment "Toast: the cast target is not water (#2930)."
    }
    CustomStringTableItem "{BF92FABD9F5CC662}" {
     Id "LF-Fishing_Range"
     Target_en_us "Too far to cast."
     Comment "Toast: the cast target is beyond the rod range (#2930)."
    }
    CustomStringTableItem "{8A16D2B2E04DC5C5}" {
     Id "LF-Fishing_Rate"
     Target_en_us "Slow down. You are casting too fast."
     Comment "Toast: more casts in a minute than allowed (#2930)."
    }
    CustomStringTableItem "{A790DD90B5BCFA87}" {
     Id "LF-Fishing_Title"
     Target_en_us "Fishing"
     Comment "Toast title for every fishing message (#2930)."
    }
    CustomStringTableItem "{FA4E9EE301A756F1}" {
     Id "LF-Fruit_Apples"
     Target_en_us "apples"
     Target_es_es "manzanas"
     Comment "Fruit name, plural lowercase, used inside a sentence"
    }
    CustomStringTableItem "{1D84C759982BB357}" {
     Id "LF-Fruit_Berries"
     Target_en_us "berries"
     Target_es_es "bayas"
     Comment "Fruit name, plural lowercase, used inside a sentence"
    }
    CustomStringTableItem "{F4B9336DFFCD7180}" {
     Id "LF-Fruit_Plums"
     Target_en_us "plums"
     Target_es_es "ciruelas"
     Comment "Fruit name, plural lowercase, used inside a sentence"
    }
    CustomStringTableItem "{57F981BB07357D29}" {
     Id "LF-FuelClerk_Name"
     Target_en_us "Fuel attendant"
     Comment "Speaker name of the fuel station attendant in a conversation."
    }
    CustomStringTableItem "{5BF4FCC1A15400BB}" {
     Id "LF-FuelClerk_Talk"
     Target_en_us "Talk to the attendant"
     Comment "Action on the fuel station attendant: opens the conversation."
    }
    CustomStringTableItem "{3330B0000000F004}" {
     Id "LF-FuelHaul_Delivered"
     Target_en_us "Delivered: %1."
     Comment "Toast after a pump-in. %1 = litres that arrived and the cash paid, for example 300 L, $300."
    }
    CustomStringTableItem "{3330B0000000F001}" {
     Id "LF-FuelHaul_Load"
     Target_en_us "Load the tanker"
     Comment "Action at the fuel depot: buys a tanker load at the wholesale price into the tanker parked beside it."
    }
    CustomStringTableItem "{3330B0000000F003}" {
     Id "LF-FuelHaul_Loaded"
     Target_en_us "Tanker loaded: %1."
     Comment "Toast after a depot load. %1 = litres and the cash charged, for example 500 L, $300."
    }
    CustomStringTableItem "{3330B0000000F005}" {
     Id "LF-FuelHaul_NoTanker"
     Target_en_us "Park the tanker next to the point."
     Comment "Toast: no vehicle with a cargo tank is within reach of the depot or the pump."
    }
    CustomStringTableItem "{3330B0000000F009}" {
     Id "LF-FuelHaul_NotBought"
     Target_en_us "That fuel was not bought at the depot."
     Comment "Toast: the tanker holds fuel, but none of it is a load bought at the depot, so the station takes nothing and pays nothing."
    }
    CustomStringTableItem "{3330B0000000F002}" {
     Id "LF-FuelHaul_PumpIn"
     Target_en_us "Pump into the station tanks"
     Comment "Action at a fuel pump: moves the tanker's load into the station stock and pays for the litres that arrived."
    }
    CustomStringTableItem "{3330B0000000F008}" {
     Id "LF-FuelHaul_StationFull"
     Target_en_us "The station tanks are full."
     Comment "Toast: the station has no room, so nothing was pumped and nothing is paid."
    }
    CustomStringTableItem "{3330B0000000F007}" {
     Id "LF-FuelHaul_TankerEmpty"
     Target_en_us "The tanker is empty."
     Comment "Toast: the tanker holds no fuel to pump into the station."
    }
    CustomStringTableItem "{3330B0000000F006}" {
     Id "LF-FuelHaul_TankerFull"
     Target_en_us "The tanker is already full."
     Comment "Toast: the tanker has no room for a litre at the depot."
    }
    CustomStringTableItem "{3328B0000000F001}" {
     Id "LF-Fuel_Buy"
     Target_en_us "Fill the tank"
     Comment "Action on a fuel pump that sells from stock: fills the nearest vehicle and charges cash."
    }
    CustomStringTableItem "{3328B0000000F004}" {
     Id "LF-Fuel_Dry"
     Target_en_us "The tanks are dry. The depot truck is due."
     Comment "Toast: the station has no fuel to sell until a delivery arrives."
    }
    CustomStringTableItem "{3328B0000000F008}" {
     Id "LF-Fuel_Failed"
     Target_en_us "The pump jams. Try again."
     Comment "Toast: the sale could not be completed (a logged fault)."
    }
    CustomStringTableItem "{3328B0000000F003}" {
     Id "LF-Fuel_Filled"
     Target_en_us "Tank filled: %1."
     Comment "Toast after a fill. %1 = litres and the cash charged, for example 30 L, $60."
    }
    CustomStringTableItem "{3328B0000000F005}" {
     Id "LF-Fuel_NoMoney"
     Target_en_us "You do not have the cash for a liter."
     Comment "Toast: the buyer cannot pay for one litre."
    }
    CustomStringTableItem "{3328B0000000F007}" {
     Id "LF-Fuel_NoVehicle"
     Target_en_us "Park a vehicle next to the pump."
     Comment "Toast: no vehicle is within reach of the pump."
    }
    CustomStringTableItem "{3328B0000000F006}" {
     Id "LF-Fuel_TankFull"
     Target_en_us "This tank is already full."
     Comment "Toast: the vehicle at the pump has no room for fuel."
    }
    CustomStringTableItem "{3328B0000000F002}" {
     Id "LF-Fuel_Title"
     Target_en_us "Fuel station"
     Comment "Toast title for a pump sale or refusal."
    }
    CustomStringTableItem "{251C7BC1C7871641}" {
     Id "LF-FurnitureStorage_Open"
     Target_en_us "Open"
     Comment "Action on a placed storage furniture (wardrobe, dresser, chest, fridge): opens its inventory for the player who placed it."
    }
    CustomStringTableItem "{C7FECEA5DD6D8719}" {
     Id "LF-Furniture_Armchair_Name"
     Target_en_us "Armchair"
     Comment "Inventory name of the carried armchair a player places in their own house."
    }
    CustomStringTableItem "{FE312113D8FACB13}" {
     Id "LF-Furniture_BedDouble_Name"
     Target_en_us "Double bed"
     Comment "Inventory name of the carried double bed a player places in their own house."
    }
    CustomStringTableItem "{B878C319D0B97054}" {
     Id "LF-Furniture_Bed_Name"
     Target_en_us "Bed"
     Comment "Inventory name of the carried bed a player places in their own house."
    }
    CustomStringTableItem "{DC940571B98F90C6}" {
     Id "LF-Furniture_Bookcase_Name"
     Target_en_us "Bookcase"
     Comment "Inventory name of the carried bookcase a player places in their own house."
    }
    CustomStringTableItem "{E5E20ACD90437F84}" {
     Id "LF-Furniture_Chair_Name"
     Target_en_us "Chair"
     Comment "Inventory name of the carried chair a player places in their own house (#2654)."
    }
    CustomStringTableItem "{E66FFF580815DAF0}" {
     Id "LF-Furniture_Chest_Name"
     Target_en_us "Chest"
     Comment "Inventory name of the carried chest a player places in their own house."
    }
    CustomStringTableItem "{1F2E04D27FF3273B}" {
     Id "LF-Furniture_CoffeeTable_Name"
     Target_en_us "Coffee table"
     Comment "Inventory name of the carried coffee table a player places in their own house."
    }
    CustomStringTableItem "{3F8B1F03F147CD0D}" {
     Id "LF-Furniture_Crate_Name"
     Target_en_us "Crate"
     Comment "Inventory name of the carried crate a player places in their own house."
    }
    CustomStringTableItem "{B9313FCEA8888FFE}" {
     Id "LF-Furniture_Cupboard_Name"
     Target_en_us "Cupboard"
     Comment "Inventory name of the carried cupboard a player places in their own house."
    }
    CustomStringTableItem "{5AAEAA90F687545E}" {
     Id "LF-Furniture_Dresser_Name"
     Target_en_us "Dresser"
     Comment "Inventory name of the carried dresser a player places in their own house."
    }
    CustomStringTableItem "{31482C09BA882977}" {
     Id "LF-Furniture_Fridge_Name"
     Target_en_us "Fridge"
     Comment "Inventory name of the carried fridge a player places in their own house."
    }
    CustomStringTableItem "{E5CB7A6F0B0F99C5}" {
     Id "LF-Furniture_KitchenUnit_Name"
     Target_en_us "Kitchen unit"
     Comment "Inventory name of the carried kitchen unit a player places in their own house."
    }
    CustomStringTableItem "{100E11EBB38DD2B9}" {
     Id "LF-Furniture_Lamp_Name"
     Target_en_us "Lamp"
     Comment "Inventory name of the carried lamp a player places in their own house (#2654)."
    }
    CustomStringTableItem "{7221176D14502441}" {
     Id "LF-Furniture_Mirror_Name"
     Target_en_us "Mirror"
     Comment "Inventory name of the carried mirror a player places in their own house."
    }
    CustomStringTableItem "{4A7D81902F5C51F8}" {
     Id "LF-Furniture_Picture_Name"
     Target_en_us "Picture"
     Comment "Inventory name of the carried picture a player places in their own house."
    }
    CustomStringTableItem "{B6724122FB6A0AE3}" {
     Id "LF-Furniture_Plant_Name"
     Target_en_us "Potted plant"
     Comment "Inventory name of the carried potted plant a player places in their own house."
    }
    CustomStringTableItem "{BE28916EA5BD4827}" {
     Id "LF-Furniture_Radio_Name"
     Target_en_us "Radio"
     Comment "Inventory name of the carried radio a player places in their own house."
    }
    CustomStringTableItem "{9B0558B964B0A890}" {
     Id "LF-Furniture_Rug_Name"
     Target_en_us "Rug"
     Comment "Inventory name of the carried rug a player places in their own house."
    }
    CustomStringTableItem "{272F0629B6A6B098}" {
     Id "LF-Furniture_Shelf_Name"
     Target_en_us "Shelf"
     Comment "Inventory name of the carried shelf a player places in their own house (#2654)."
    }
    CustomStringTableItem "{25B6D83F8F0B1FEA}" {
     Id "LF-Furniture_Sofa_Name"
     Target_en_us "Sofa"
     Comment "Inventory name of the carried sofa a player places in their own house."
    }
    CustomStringTableItem "{D3D2D71766E3DB37}" {
     Id "LF-Furniture_Stool_Name"
     Target_en_us "Stool"
     Comment "Inventory name of the carried stool a player places in their own house."
    }
    CustomStringTableItem "{82829910DC892680}" {
     Id "LF-Furniture_Stove_Name"
     Target_en_us "Stove"
     Comment "Inventory name of the carried stove a player places in their own house."
    }
    CustomStringTableItem "{CD0C9FCE1E77121B}" {
     Id "LF-Furniture_Table_Name"
     Target_en_us "Table"
     Comment "Inventory name of the carried table a player places in their own house (#2654)."
    }
    CustomStringTableItem "{319DEB246B36444C}" {
     Id "LF-Furniture_Tv_Name"
     Target_en_us "Television"
     Comment "Inventory name of the carried television a player places in their own house."
    }
    CustomStringTableItem "{252218056270237E}" {
     Id "LF-Furniture_Wardrobe_Name"
     Target_en_us "Wardrobe"
     Comment "Inventory name of the carried wardrobe a player places in their own house."
    }
    CustomStringTableItem "{2D489E9F3516FB40}" {
     Id "LF-Furniture_Workbench_Name"
     Target_en_us "Workbench"
     Comment "Inventory name of the carried workbench a player places in their own house."
    }
    CustomStringTableItem "{D4DD0BA4533222B8}" {
     Id "LF-GangRadio_NotMember"
     Target_en_us "Gang members only"
     Comment "Reason the gang radio cannot be tuned: the carrier is in no gang (#4221)."
    }
    CustomStringTableItem "{7C98C20B33E1A120}" {
     Id "LF-GangRadio_Tune"
     Target_en_us "Tune to gang channel"
     Comment "Action on a carried gang radio: puts it on the gang band (#4221)."
    }
    CustomStringTableItem "{3CEA2232FA244C93}" {
     Id "LF-Garage_Blue"
     Target_en_us "Blue"
     Comment "Garage catalog blue color field."
    }
    CustomStringTableItem "{F35E37BAE85B4ACA}" {
     Id "LF-Garage_Braking"
     Target_en_us "Braking"
     Comment "Garage catalog braking field."
    }
    CustomStringTableItem "{86AD47C0A38CE390}" {
     Id "LF-Garage_BuyHint"
     Target_en_us "Park it at the garage next to the lot and store it to keep it. A car left on the street at a server restart is towed to the impound lot behind the garage."
     Comment "Shown after a vehicle purchase: the garage-it-to-keep-it rule"
    }
    CustomStringTableItem "{C97AAED75F4A4D52}" {
     Id "LF-Garage_BuySlot"
     Target_en_us "Buy a garage slot"
     Comment "Garage desk action and toast title (#2656): buy one more garage slot with carried cash."
    }
    CustomStringTableItem "{58906F560E794544}" {
     Id "LF-Garage_EmptyCargo"
     Target_en_us "Empty the vehicle's storage first"
     Comment "Garage store refused (#2411): the vehicle still holds items, and a stored vehicle keeps no cargo."
    }
    CustomStringTableItem "{7DACF581EBBB4514}" {
     Id "LF-Garage_Engine"
     Target_en_us "Engine"
     Comment "Garage catalog engine field."
    }
    CustomStringTableItem "{0DD2FE2B33C34AAC}" {
     Id "LF-Garage_Exit"
     Target_en_us "Exit"
     Comment "Garage catalog close button."
    }
    CustomStringTableItem "{974806D9A0704F4D}" {
     Id "LF-Garage_Green"
     Target_en_us "Green"
     Comment "Garage catalog green color field."
    }
    CustomStringTableItem "{B78D82A085EA485D}" {
     Id "LF-Garage_Handling"
     Target_en_us "Handling"
     Comment "Garage catalog handling field."
    }
    CustomStringTableItem "{66D8858FA06871D7}" {
     Id "LF-Garage_LedgerClose"
     Target_en_us "Put it back"
     Comment "Closes the garage ledger card"
    }
    CustomStringTableItem "{03E3D61F9E999AD2}" {
     Id "LF-Garage_LedgerEmpty"
     Target_en_us "No vehicles are registered to you."
     Comment "Ledger card line when the reader owns no stored or impounded vehicle"
    }
    CustomStringTableItem "{5CDA9148049FE180}" {
     Id "LF-Garage_LedgerFee"
     Target_en_us "Release fee at the impound desk behind the garage: {currency:sym}%1 in cash."
     Comment "Ledger card line when a vehicle is impounded. %1 is the fee"
    }
    CustomStringTableItem "{A95D5C7E27177491}" {
     Id "LF-Garage_LedgerImpounded"
     Target_en_us "at the impound lot"
     Comment "Ledger row place: the vehicle was left on the street at a restart"
    }
    CustomStringTableItem "{F48BC16138BA72D1}" {
     Id "LF-Garage_LedgerRow"
     Target_en_us "%1, plate %2: %3"
     Comment "Ledger row. %1 is the model, %2 the plate, %3 where the vehicle is (in the garage, at the impound lot)"
    }
    CustomStringTableItem "{B2FF1D7EF35A27D7}" {
     Id "LF-Garage_LedgerStored"
     Target_en_us "in the garage"
     Comment "Ledger row place: the vehicle is stored and survives restarts"
    }
    CustomStringTableItem "{D033165293830B25}" {
     Id "LF-Garage_LedgerStreet"
     Target_en_us "on the street, towed to the impound lot at the next restart unless garaged"
     Comment "Ledger row place: the vehicle is out in the world"
    }
    CustomStringTableItem "{B91E50924997458C}" {
     Id "LF-Garage_LedgerTitle"
     Target_en_us "Garage Ledger"
     Comment "Title of the garage ledger card"
    }
    CustomStringTableItem "{9E09C42DCA344AD1}" {
     Id "LF-Garage_Next"
     Target_en_us "Next"
     Comment "Garage catalog action that moves to the next vehicle."
    }
    CustomStringTableItem "{844A5DFB122EDF8C}" {
     Id "LF-Garage_NothingStored"
     Target_en_us "You have no vehicle in the garage."
     Comment "Retrieve refused: nothing stored"
    }
    CustomStringTableItem "{4D580A2D0F314D42}" {
     Id "LF-Garage_Occupied"
     Target_en_us "Everyone must step out of the vehicle first"
     Comment "Garage store refused (#2411): someone is still seated in the vehicle."
    }
    CustomStringTableItem "{A103BFA88FF840F0}" {
     Id "LF-Garage_PadBusy"
     Target_en_us "The pad in front of the garage is blocked. Move the car that is standing there."
     Comment "Retrieve refused: every garage pad is occupied"
    }
    CustomStringTableItem "{01D3310E68104252}" {
     Id "LF-Garage_Previous"
     Target_en_us "Previous"
     Comment "Garage catalog action that moves to the previous vehicle."
    }
    CustomStringTableItem "{D33C625BF93B46A7}" {
     Id "LF-Garage_Price"
     Target_en_us "Cash price"
     Comment "Garage catalog price field. The row shows the amount."
    }
    CustomStringTableItem "{7EC51FB11D0B1C50}" {
     Id "LF-Garage_ReadLedger"
     Target_en_us "Read the garage ledger"
     Comment "Action at the garage and the impound desk that opens the ledger card of your vehicles"
    }
    CustomStringTableItem "{8CF0A65794DC448C}" {
     Id "LF-Garage_Red"
     Target_en_us "Red"
     Comment "Garage catalog red color field."
    }
    CustomStringTableItem "{EFE87FE08B769414}" {
     Id "LF-Garage_Retrieved"
     Target_en_us "%1 is waiting on the pad in front of the garage."
     Comment "Retrieve succeeded. %1 is the plate"
    }
    CustomStringTableItem "{51F194E2FE80AEC0}" {
     Id "LF-Garage_SlotBought"
     Target_en_us "Slot bought. Your garage now holds %1 vehicles."
     Comment "Garage slot bought (#2656). %1 is the new slot cap."
    }
    CustomStringTableItem "{C1E5F8750E4D23B8}" {
     Id "LF-Garage_SlotFailed"
     Target_en_us "The slot could not be bought. Nothing was taken."
     Comment "Garage slot refused (#2656): the cash could not be taken in full and was handed back."
    }
    CustomStringTableItem "{015CF51C90ECA5D4}" {
     Id "LF-Garage_SlotNotAlive"
     Target_en_us "You cannot buy a slot right now."
     Comment "Garage slot refused (#2656): the buyer is dead or downed."
    }
    CustomStringTableItem "{AC1FD7B3F3DBE446}" {
     Id "LF-Garage_SlotShortCash"
     Target_en_us "The next slot costs {currency:sym}%1 in cash. You do not carry enough."
     Comment "Garage slot refused (#2656): carried cash is short. %1 is the price."
    }
    CustomStringTableItem "{4F4F48DC707A84B4}" {
     Id "LF-Garage_SlotsFull"
     Target_en_us "Your garage is full: %1 vehicles at most. Retrieve one before storing another."
     Comment "Garage store refused (#2655): the owner's stored vehicles fill every garage slot. %1 is the cap."
    }
    CustomStringTableItem "{E61274B7B23F4466}" {
     Id "LF-Garage_SlotsMax"
     Target_en_us "Your garage is at the most it can hold: %1 slots."
     Comment "Garage slot refused (#2656): the character holds the max. %1 is the max."
    }
    CustomStringTableItem "{A1C4E87B62D94F05}" {
     Id "LF-Garage_StepOut"
     Target_en_us "Step out of the vehicle first"
     Comment "Garage store refused because the player is still seated."
    }
    CustomStringTableItem "{693771B3C5A1420C}" {
     Id "LF-Garage_Storage"
     Target_en_us "Storage"
     Comment "Garage catalog storage field."
    }
    CustomStringTableItem "{405F3EF53D4704D5}" {
     Id "LF-Garage_StoreHere"
     Target_en_us "Store in garage"
     Comment "Action on the player's own car when it stands within the garage radius. Same rules as the garage counter."
    }
    CustomStringTableItem "{FC90DC9BBE2A0562}" {
     Id "LF-Garage_Stored"
     Target_en_us "Stored in the garage: %1. Keep your key."
     Comment "Store succeeded. %1 is the plate"
    }
    CustomStringTableItem "{5635430D41DA4AF6}" {
     Id "LF-Garage_Title"
     Target_en_us "Garage"
     Comment "Garage catalog title."
    }
    CustomStringTableItem "{2D4F8FB9A33C7F28}" {
     Id "LF-Garage_VehicleTooFar"
     Target_en_us "Bring the vehicle to the garage first"
     Comment "Garage store refused because the owned vehicle is not parked at the garage the player is using"
    }
    CustomStringTableItem "{F91197344A75BE03}" {
     Id "LF-Garbage_EmptyBin"
     Target_en_us "Empty trash can"
     Comment "Action on a street bin during a garbage round."
    }
    CustomStringTableItem "{D036ADB55B9871A1}" {
     Id "LF-Garbage_EmptyBinDesc"
     Target_en_us "Bag the trash onto the truck."
     Comment "Description of the empty-bin action."
    }
    CustomStringTableItem "{A9C0133F738C8E8F}" {
     Id "LF-Garbage_NoBins"
     Target_en_us "There are no bins to collect today."
     Comment "Toast: the world has no bin to build a route from."
    }
    CustomStringTableItem "{0CF2638908349C68}" {
     Id "LF-Garbage_NoRound"
     Target_en_us "You are not on a garbage round. Take one at the depot."
     Comment "Toast: a bin or dump action with no round open."
    }
    CustomStringTableItem "{2F0D378D85A3F2CB}" {
     Id "LF-Garbage_NoTruck"
     Target_en_us "The company truck is not available."
     Comment "Toast: the truck did not spawn."
    }
    CustomStringTableItem "{D7C496F1D48B0E59}" {
     Id "LF-Garbage_NotEnough"
     Target_en_us "There are still bins to empty before you can tip."
     Comment "Toast: tip asked before every bin is done."
    }
    CustomStringTableItem "{A93E6E6C5E0A44F8}" {
     Id "LF-Garbage_NotOnRoute"
     Target_en_us "That trash can is not on your route."
     Comment "Toast: a bin that is not one of the round bins."
    }
    CustomStringTableItem "{E1CB7F2381C5C2E1}" {
     Id "LF-Garbage_OutOfOrder"
     Target_en_us "Not that one yet: empty the bins in order."
     Comment "Toast: a route bin emptied out of order."
    }
    CustomStringTableItem "{27AEAA0E0829E94D}" {
     Id "LF-Garbage_Paid"
     Target_en_us "Round done. You were paid {currency:sym}%1."
     Comment "Toast: the garbage round paid. %1 = the cash."
    }
    CustomStringTableItem "{22366B508087A675}" {
     Id "LF-Garbage_Start"
     Target_en_us "Take the garbage round"
     Comment "Action at the depot desk: start the round and take the company truck."
    }
    CustomStringTableItem "{AF715C965D4C2353}" {
     Id "LF-Garbage_StartDesc"
     Target_en_us "Empty the bins on your route, then tip at the dump."
     Comment "Description of the depot action."
    }
    CustomStringTableItem "{00D9E7AED62B6134}" {
     Id "LF-Garbage_Started"
     Target_en_us "The truck is outside. Empty the bins nearest the depot in order, then tip the load at the dump."
     Comment "Toast: the round began."
    }
    CustomStringTableItem "{CC20960FA42008DF}" {
     Id "LF-Garbage_Tip"
     Target_en_us "Tip the load"
     Comment "Action at the dump: end the round and get paid."
    }
    CustomStringTableItem "{D3D5F43917D0BCAB}" {
     Id "LF-Garbage_TipDesc"
     Target_en_us "Tip the trash and collect your pay."
     Comment "Description of the tip action."
    }
    CustomStringTableItem "{1240A91D844B207D}" {
     Id "LF-Garbage_Title"
     Target_en_us "Garbage round"
     Comment "Toast title for every garbage-round message."
    }
    CustomStringTableItem "{4B7E1A90C3D5F268}" {
     Id "LF-Garbage_TooFast"
     Target_en_us "You are done too quickly. Take a breather, then tip the load."
     Comment "Toast: tip asked before the round's minimum time has passed."
    }
    CustomStringTableItem "{4D1D1033459A6CDB}" {
     Id "LF-Garbage_TruckFar"
     Target_en_us "Bring the truck to the dump first."
     Comment "Toast: tip asked with the truck away from the dump."
    }
    CustomStringTableItem "{DDFD7A1F8207F359}" {
     Id "LF-Gather_Action"
     Target_en_us "Gather %1"
     Comment "Action name for gathering a resource. %1 = item name"
    }
    CustomStringTableItem "{C978A84E212BD638}" {
     Id "LF-Gather_Empty"
     Target_en_us "Picked clean for now"
     Comment "Gather refused on the client: the spot is out of stock (#2409)"
    }
    CustomStringTableItem "{E1BA56AC133E9004}" {
     Id "LF-Gather_EmptyWait"
     Target_en_us "Picked clean. More in about %1 min."
     Comment "Toast when a gather spot is out of stock. %1 = minutes until the next one grows back (#2409)"
    }
    CustomStringTableItem "{8D3B6F20A4C17E95}" {
     Id "LF-Gather_NeedPickaxe"
     Target_en_us "You need a pickaxe to mine this"
     Comment "Gather refused: a rock, no pickaxe held (#2706)"
    }
    CustomStringTableItem "{5C1E0A7D9B2F4E61}" {
     Id "LF-Gather_NeedShovel"
     Target_en_us "You need a shovel to dig here"
     Comment "Gather refused: a digging spot, no shovel held (#2706)"
    }
    CustomStringTableItem "{44A28FD4E95EA8C3}" {
     Id "LF-Gather_RequiresItem"
     Target_en_us "Requires the right tool"
     Comment "Gather refused: the required tool is not in the inventory"
    }
    CustomStringTableItem "{FDA4450EBA23EE4D}" {
     Id "LF-Gather_RequiresItemInHands"
     Target_en_us "Requires the right tool in hand"
     Comment "Gather refused: the required tool is not held"
    }
    CustomStringTableItem "{3156A1C07E5B2D91}" {
     Id "LF-Gather_Toast"
     Target_en_us "%1 x%2"
     Comment "Toast after a gather or a process (#3156). %1 = item name, %2 = amount"
    }
    CustomStringTableItem "{3156C3E290A7D4B3}" {
     Id "LF-Gather_ToastPrice"
     Target_en_us "The trader pays {currency:sym}%1 for %2."
     Comment "Second sentence of a gather or process toast when a trader buys the item or its product (#3156). %1 = price, %2 = item name"
    }
    CustomStringTableItem "{3156B2D18F6C3EA2}" {
     Id "LF-Gather_ToastXp"
     Target_en_us "%1 x%2 (+%3 XP)"
     Comment "Toast after a gather that paid job XP (#3156). %1 = item name, %2 = amount, %3 = XP paid"
    }
    CustomStringTableItem "{2969D3779A79F0EA}" {
     Id "LF-Gather_Unknown"
     Target_en_us "resource"
     Comment "Gather action fallback when the gathered item has no name"
    }
    CustomStringTableItem "{90AE356E2AC3F7C8}" {
     Id "LF-Gather_Wait"
     Target_en_us "Wait %1 seconds"
     Comment "Gather refused: the resource is restocking. %1 = seconds"
    }
    CustomStringTableItem "{D83A61F4B05C92E7}" {
     Id "LF-Gossip_Overheard"
     Target_en_us "You overhear the gossips. A new place is marked on your map."
     Comment "Toast: a rumor overheard on the gossip bench; its place is now heard of (#4462)."
    }
    CustomStringTableItem "{7C1E0B5A92D4F3A6}" {
     Id "LF-Gossip_Title"
     Target_en_us "Overheard"
     Comment "Toast title when a player sitting on the gossip bench overhears a rumor (#4462)."
    }
    CustomStringTableItem "{73D0C4773B5E967E}" {
     Id "LF-Grace_Refused"
     Target_en_us "A new arrival is under a short grace: no hold-up, robbery or arrest for now."
     Comment "Toast when a hold-up, robbery or arrest is refused because a character is in its new-character grace."
    }
    CustomStringTableItem "{A97CA5DA23A8F11C}" {
     Id "LF-Grace_Title"
     Target_en_us "Grace period"
     Comment "Toast title for the new-character grace refusal."
    }
    CustomStringTableItem "{B0C49E2CC5A91698}" {
     Id "LF-Handbook_Close"
     Target_en_us "Close"
     Comment "Handbook screen: the button that puts the handbook away (#1773)."
    }
    CustomStringTableItem "{137851841CD11F98}" {
     Id "LF-Handbook_Contents"
     Target_en_us "Back to the pages"
     Comment "Handbook screen: the button on a page that returns to the list of pages (#1773)."
    }
    CustomStringTableItem "{D2E4378EDC9C7624}" {
     Id "LF-Handbook_Empty"
     Target_en_us "The handbook has no pages yet."
     Comment "Handbook screen: shown when no page can be read (#1773)."
    }
    CustomStringTableItem "{B368DC21838D0FEE}" {
     Id "LF-Handbook_PauseButton"
     Target_en_us "Handbook"
     Comment "Pause menu: the button that opens the handbook (#1773)."
    }
    CustomStringTableItem "{29A96057080889A7}" {
     Id "LF-Handbook_Pick"
     Target_en_us "Pick a page to read."
     Comment "Handbook screen: the line above the list of pages (#1773)."
    }
    CustomStringTableItem "{5B3900D36A58CBE9}" {
     Id "LF-Handbook_Read"
     Target_en_us "Read"
     Comment "Handbook screen: the button on a page row in the list (#1773)."
    }
    CustomStringTableItem "{37A4E1474FFDB190}" {
     Id "LF-Handbook_ReadBody1"
     Target_en_us "The handbook is a short set of pages. Each page covers one topic and you can come back to it at any time."
     Comment "Handbook page body, first paragraph of how to read it."
    }
    CustomStringTableItem "{F1367AFD4DBABE64}" {
     Id "LF-Handbook_ReadBody2"
     Target_en_us "Pages are listed in reading order. If a page is missing, ask a staff member."
     Comment "Handbook page body, second paragraph of how to read it."
    }
    CustomStringTableItem "{C66A836BB94AF2C9}" {
     Id "LF-Handbook_ReadTitle"
     Target_en_us "How to read this handbook"
     Comment "Handbook page title: how to use the handbook."
    }
    CustomStringTableItem "{5A806A961C8BB7E6}" {
     Id "LF-Handbook_RulesBody1"
     Target_en_us "The server rules are set by this server. Read them before you play, and ask a staff member if one is unclear."
     Comment "Handbook page body, where to find the rules."
    }
    CustomStringTableItem "{D249EF1289AA15DE}" {
     Id "LF-Handbook_RulesTitle"
     Target_en_us "Where to find the rules"
     Comment "Handbook page title: where the server rules live."
    }
    CustomStringTableItem "{2B856ABF8687043C}" {
     Id "LF-Handbook_Title"
     Target_en_us "HANDBOOK"
     Comment "Printed heading of the handbook screen (#1773)."
    }
    CustomStringTableItem "{7B4E0C91A2D63F71}" {
     Id "LF-HandsUp_Hint"
     Target_en_us "Press F7 to raise your hands. Ignore this and nothing changes."
     Comment "HUD prompt: the key hint (default key F7)."
    }
    CustomStringTableItem "{7B4E0C91A2D63F72}" {
     Id "LF-HandsUp_Keybind"
     Target_en_us "Hands up (answer the prompt, or lower them)"
     Comment "Keybind menu entry for the hands-up action."
    }
    CustomStringTableItem "{339D1913E2EBFE37}" {
     Id "LF-HandsUp_NotGivenUp"
     Target_en_us "They haven't given up."
     Comment "Search or robbery of a player refused: the target has neither hands up nor cuffs on (#3399)."
    }
    CustomStringTableItem "{7B4E0C91A2D63F70}" {
     Id "LF-HandsUp_Title"
     Target_en_us "Put your hands up?"
     Comment "HUD prompt to a player held up or aimed at: the title."
    }
    CustomStringTableItem "{57403D2F7D7CFD8A}" {
     Id "LF-Handset_CallMedic"
     Target_en_us "Radio for a doctor"
     Comment "Radio set action: call a medic."
    }
    CustomStringTableItem "{544D193A3E8A461A}" {
     Id "LF-Handset_CallPolice"
     Target_en_us "Radio for an officer"
     Comment "Radio set action: call the police. The engine renders a prefab action name itself, so the text holds no term placeholder (validate-term-sinks)."
    }
    CustomStringTableItem "{0350E5CBCEDFF757}" {
     Id "LF-Handset_CallTaxi"
     Target_en_us "Radio for a cab"
     Comment "Radio set action: call a taxi."
    }
    CustomStringTableItem "{2F049E3EF139D9CE}" {
     Id "LF-Handset_Connected"
     Target_en_us "You got through on the radio."
     Comment "Toast: the radio set call reached somebody on duty."
    }
    CustomStringTableItem "{6A9D4C7550B7168A}" {
     Id "LF-Handset_NoCash"
     Target_en_us "You need cash for the call fee."
     Comment "Radio set call refused: not enough carried cash for the fee."
    }
    CustomStringTableItem "{C1AB8F125674BBDD}" {
     Id "LF-Handset_NotCarried"
     Target_en_us "Carry the radio set to call on it."
     Comment "Radio set call refused: the radio is not in the caller inventory."
    }
    CustomStringTableItem "{6DD229FA5A46FBA5}" {
     Id "LF-Handset_Summary_Call"
     Target_en_us "Called in on a radio set"
     Comment "Service board summary: the caller used a carried radio set."
    }
    CustomStringTableItem "{BE2EAACE7DF5E3EF}" {
     Id "LF-Handset_Title"
     Target_en_us "Radio set"
     Comment "Toast title for calls placed on a carried radio set."
    }
    CustomStringTableItem "{3656A1B2C3D4000B}" {
     Id "LF-Harvest_BoardAlreadyPosted"
     Target_en_us "This field already has its contract"
     Comment "Toast (#3656): one contract per field."
    }
    CustomStringTableItem "{3656A1B2C3D4000E}" {
     Id "LF-Harvest_BoardKitFailed"
     Target_en_us "The rental kit did not come out. Your deposit is back"
     Comment "Toast (#3656): the vehicle did not spawn."
    }
    CustomStringTableItem "{3656A1B2C3D40008}" {
     Id "LF-Harvest_BoardNoField"
     Target_en_us "No wheat field belongs to this board"
     Comment "Toast (#3656): no field near the board."
    }
    CustomStringTableItem "{3656A1B2C3D4000D}" {
     Id "LF-Harvest_BoardNoKit"
     Target_en_us "You have no rental kit to return"
     Comment "Toast (#3656): no rental kit out."
    }
    CustomStringTableItem "{3656A1B2C3D40009}" {
     Id "LF-Harvest_BoardNotPosted"
     Target_en_us "Post the contract first"
     Comment "Toast (#3656): take or return with no contract on the field."
    }
    CustomStringTableItem "{3656A1B2C3D4000A}" {
     Id "LF-Harvest_BoardNotRipe"
     Target_en_us "This field is already cut"
     Comment "Toast (#3656): nothing left to harvest."
    }
    CustomStringTableItem "{3656A1B2C3D4000C}" {
     Id "LF-Harvest_BoardNotTaker"
     Target_en_us "Only whoever took the contract can return the kit"
     Comment "Toast (#3656): a stranger tried to return the kit."
    }
    CustomStringTableItem "{3656A1B2C3D40001}" {
     Id "LF-Harvest_BoardPost"
     Target_en_us "Post contract"
     Comment "Prompt (#3656): the farm board posts the field's contract."
    }
    CustomStringTableItem "{3656A1B2C3D40005}" {
     Id "LF-Harvest_BoardPosted"
     Target_en_us "Contract posted for this field"
     Comment "Toast (#3656): the field now has its contract."
    }
    CustomStringTableItem "{3656A1B2C3D40004}" {
     Id "LF-Harvest_BoardReturn"
     Target_en_us "Return kit"
     Comment "Prompt (#3656): bring the rental kit back for the deposit."
    }
    CustomStringTableItem "{3656A1B2C3D40002}" {
     Id "LF-Harvest_BoardTakeOwn"
     Target_en_us "Take with own reaper"
     Comment "Prompt (#3656): take the contract with the reaper from the owned list."
    }
    CustomStringTableItem "{3656A1B2C3D40003}" {
     Id "LF-Harvest_BoardTakeRental"
     Target_en_us "Take with rental kit"
     Comment "Prompt (#3656): take the contract with a rented tractor and reaper."
    }
    CustomStringTableItem "{3656A1B2C3D40006}" {
     Id "LF-Harvest_BoardTakenOwn"
     Target_en_us "Contract taken. Hitch your reaper at the rack and cut the field"
     Comment "Toast (#3656): the own-reaper route started."
    }
    CustomStringTableItem "{3656A1B2C3D40007}" {
     Id "LF-Harvest_BoardTakenRental"
     Target_en_us "Contract taken. The rental kit stands by the board"
     Comment "Toast (#3656): the rental kit spawned."
    }
    CustomStringTableItem "{3297A1B2C3D40008}" {
     Id "LF-Harvest_ContractKitFar"
     Target_en_us "Bring the kit back to the board first"
     Comment "Toast (#3297): the rental kit is not at the board."
    }
    CustomStringTableItem "{3297A1B2C3D40004}" {
     Id "LF-Harvest_ContractLapsed"
     Target_en_us "The contract lapsed. The kit is taken away and the deposit is kept"
     Comment "Toast (#3297): the timeout passed with the kit still out."
    }
    CustomStringTableItem "{3297A1B2C3D40007}" {
     Id "LF-Harvest_ContractNoCash"
     Target_en_us "The kit deposit is {currency:sym}300. You do not have it"
     Comment "Toast (#3297): the rental deposit could not be paid."
    }
    CustomStringTableItem "{3297A1B2C3D40006}" {
     Id "LF-Harvest_ContractNoReaper"
     Target_en_us "You own no reaper. Rent a kit or buy one at the farm supply"
     Comment "Toast (#3297): the own-reaper route without a reaper."
    }
    CustomStringTableItem "{3297A1B2C3D40002}" {
     Id "LF-Harvest_ContractPaid"
     Target_en_us "Contract done: {currency:sym}%1 bonus and farming experience"
     Comment "Toast (#3297): the field is cut to the line. %1 = the bonus paid."
    }
    CustomStringTableItem "{3297A1B2C3D40003}" {
     Id "LF-Harvest_ContractRefund"
     Target_en_us "Kit returned, {currency:sym}%1 deposit back"
     Comment "Toast (#3297): the rental kit is back at the board. %1 = the deposit refunded."
    }
    CustomStringTableItem "{3297A1B2C3D40005}" {
     Id "LF-Harvest_ContractTaken"
     Target_en_us "Someone already holds this contract"
     Comment "Toast (#3297): one taker per field."
    }
    CustomStringTableItem "{3297A1B2C3D40001}" {
     Id "LF-Harvest_ContractTitle"
     Target_en_us "Harvest contract"
     Comment "Toast title (#3297)."
    }
    CustomStringTableItem "{1C50A1F8B0E1940A}" {
     Id "LF-Harvest_HopperFull"
     Target_en_us "Hopper full, unload at the granary"
     Comment "Hint when the reaper's hopper is at its cap and cutting stops (#3285)."
    }
    CustomStringTableItem "{BF338F251412CB57}" {
     Id "LF-Harvest_RackAlreadyOut"
     Target_en_us "Your reaper is already out"
     Comment "Toast (#3295): take-out while the player's reaper is hitched somewhere."
    }
    CustomStringTableItem "{B22321ACD509B257}" {
     Id "LF-Harvest_RackDumped"
     Target_en_us "Reaper put back. The hopper was dumped: %1 kg of grain lost"
     Comment "Warning toast (#3295): put back with grain in the hopper. %1 = kilograms dumped."
    }
    CustomStringTableItem "{3355FF558340E68F}" {
     Id "LF-Harvest_RackFailed"
     Target_en_us "The reaper would not hitch"
     Comment "Toast (#3295): the hitch failed; the server log says why."
    }
    CustomStringTableItem "{DB422FE9426E93B2}" {
     Id "LF-Harvest_RackNoVehicle"
     Target_en_us "Park a UAZ469 or a Z7211 beside the rack first"
     Comment "Toast (#3295): no free hitch-list vehicle within reach of the rack."
    }
    CustomStringTableItem "{76DD952E4AE4819E}" {
     Id "LF-Harvest_RackNotOut"
     Target_en_us "You have no reaper out"
     Comment "Toast (#3295): put-back with nothing out."
    }
    CustomStringTableItem "{008EA7B9C6EDB74C}" {
     Id "LF-Harvest_RackNotOwned"
     Target_en_us "You do not own a reaper. The farm supply counter sells one"
     Comment "Toast (#3295): take-out by a player with no reaper."
    }
    CustomStringTableItem "{DBBAC35BF4F45EDA}" {
     Id "LF-Harvest_RackPutBack"
     Target_en_us "Put back reaper"
     Comment "Implement rack prompt (#3295): unhitch your reaper and return it to the rack. Also the toast title."
    }
    CustomStringTableItem "{8139653E1150737F}" {
     Id "LF-Harvest_RackPutBackDone"
     Target_en_us "Your reaper is back on the rack"
     Comment "Toast (#3295): put back with an empty hopper."
    }
    CustomStringTableItem "{5693A725630DADEF}" {
     Id "LF-Harvest_RackTakeOut"
     Target_en_us "Take out reaper"
     Comment "Implement rack prompt (#3295): hitch your reaper to a vehicle beside the rack. Also the toast title."
    }
    CustomStringTableItem "{DF31D8D79C5EF941}" {
     Id "LF-Harvest_RackTakenOut"
     Target_en_us "Your reaper is hitched on"
     Comment "Toast (#3295): the reaper is now behind the vehicle."
    }
    CustomStringTableItem "{CED0297A3041F904}" {
     Id "LF-Harvest_RackVehicleFar"
     Target_en_us "Bring the vehicle that tows your reaper to the rack"
     Comment "Toast (#3295): put-back with the towing vehicle out of reach."
    }
    CustomStringTableItem "{3E7A1E93CB13CBFC}" {
     Id "LF-Harvest_TooFast"
     Target_en_us "Too fast to cut, slow down"
     Comment "Hint when the hitched reaper is driven above its cutting speed (#3285)."
    }
    CustomStringTableItem "{75ADFCA12F6308D2}" {
     Id "LF-Harvest_UnloadBag"
     Target_en_us "Bag grain"
     Comment "Weighbridge prompt (#3294): fill grain sacks into the vehicle from the hopper. Also the toast title."
    }
    CustomStringTableItem "{43CAC0C011BC3F37}" {
     Id "LF-Harvest_UnloadBagged"
     Target_en_us "Bagged %1 kg of grain into the vehicle"
     Comment "Toast (#3294): sacks went into the vehicle storage. %1 = kilograms bagged."
    }
    CustomStringTableItem "{C79F60A8AF8029DF}" {
     Id "LF-Harvest_UnloadBelowLot"
     Target_en_us "The granary buys whole lots of %1 kg. Cut more, or bag it"
     Comment "Toast (#3294): the hopper holds less than one lot. %1 = kilograms in a lot."
    }
    CustomStringTableItem "{B7D75EE9BC51A3BA}" {
     Id "LF-Harvest_UnloadEmpty"
     Target_en_us "The hopper is empty"
     Comment "Toast (#3294): nothing to unload."
    }
    CustomStringTableItem "{78F9B96C35EFB762}" {
     Id "LF-Harvest_UnloadFailed"
     Target_en_us "The sale did not go through, the grain stays in the hopper"
     Comment "Toast (#3294): the payout failed and was rolled back."
    }
    CustomStringTableItem "{5E44FCFE9C91F4F6}" {
     Id "LF-Harvest_UnloadNoPrice"
     Target_en_us "The granary pays nothing for grain right now"
     Comment "Toast (#3294): the market does not price grain."
    }
    CustomStringTableItem "{CF02DAF30144B9D7}" {
     Id "LF-Harvest_UnloadNoReaper"
     Target_en_us "Hitch a reaper to unload grain"
     Comment "Toast (#3294): the vehicle tows no reaper."
    }
    CustomStringTableItem "{C435B184BB196816}" {
     Id "LF-Harvest_UnloadNoRoom"
     Target_en_us "No room in the vehicle for grain sacks"
     Comment "Toast (#3294): the vehicle storage took no sack."
    }
    CustomStringTableItem "{2651785AF83988E5}" {
     Id "LF-Harvest_UnloadNoVehicle"
     Target_en_us "Drive your reaper onto the weighbridge"
     Comment "Toast (#3294): the user is not in a vehicle."
    }
    CustomStringTableItem "{C831FED3CB2B1ABE}" {
     Id "LF-Harvest_UnloadNotDriver"
     Target_en_us "Only the driver can unload the reaper"
     Comment "Toast (#3294): a passenger used the prompt."
    }
    CustomStringTableItem "{DC9F11F308A8512A}" {
     Id "LF-Harvest_UnloadOutside"
     Target_en_us "Drive onto the weighbridge to unload"
     Comment "Toast (#3294): the vehicle is outside the unload box."
    }
    CustomStringTableItem "{916A8158813D3E43}" {
     Id "LF-Harvest_UnloadSell"
     Target_en_us "Sell grain"
     Comment "Weighbridge prompt (#3294): sell the reaper's hopper at the wholesale price. Also the toast title."
    }
    CustomStringTableItem "{FF63B481C8E5AA1E}" {
     Id "LF-Harvest_UnloadSold"
     Target_en_us "Sold the grain for {currency:sym}%1"
     Comment "Toast (#3294): the hopper was sold. %1 = cash paid."
    }
    CustomStringTableItem "{9306F928F7A81EBC}" {
     Id "LF-Heist_Armory_Open"
     Target_en_us "Breach the armory store"
     Comment "Action: the police armory store (#275)."
    }
    CustomStringTableItem "{8D97105F774AFD27}" {
     Id "LF-Heist_Empty"
     Target_en_us "Nothing left in there"
     Comment "Offer: the heist target is out of stock until it refills (#275)."
    }
    CustomStringTableItem "{C3E7A15B90D24F68}" {
     Id "LF-Heist_NeedPolice"
     Target_en_us "Not enough {police} on duty for this job"
     Comment "Breach charge refused on a heist job's door because fewer police are on duty than the job needs"
    }
    CustomStringTableItem "{287FBFE0C09D4B2C}" {
     Id "LF-Heist_Van_Open"
     Target_en_us "Break open the cash box"
     Comment "Action: the cash van rear box (#274)."
    }
    CustomStringTableItem "{E33FDD3D631C2068}" {
     Id "LF-HideoutStash_Desc"
     Target_en_us "Shared storage of the organization that owns this hideout."
     Comment "Inventory description of the organization hideout stash crate."
    }
    CustomStringTableItem "{003DBD3771195E72}" {
     Id "LF-HideoutStash_Name"
     Target_en_us "Hideout stash"
     Comment "Inventory title of the organization hideout stash crate."
    }
    CustomStringTableItem "{6544A393A6C50DA4}" {
     Id "LF-HideoutStash_NoOwner"
     Target_en_us "The stash is padlocked. No organization owns this hideout yet."
     Comment "Toast when a player tries to open the stash of a hideout no organization owns."
    }
    CustomStringTableItem "{0F3071065A46DE99}" {
     Id "LF-HideoutStash_NotMember"
     Target_en_us "The stash is padlocked. Only members of the organization that owns this hideout can open it. Ask a member to invite you at the organization registry."
     Comment "Toast when a player who is not in the hideout's organization tries to open its stash."
    }
    CustomStringTableItem "{71BCA78110E0B841}" {
     Id "LF-HideoutStash_Open"
     Target_en_us "Open hideout stash (members only)"
     Comment "Action on the hideout stash crate. Tells a stranger who may open it."
    }
    CustomStringTableItem "{38490B8B4615127D}" {
     Id "LF-HideoutStash_Title"
     Target_en_us "Hideout stash"
     Comment "Toast title when the hideout stash refuses a player."
    }
    CustomStringTableItem "{6A4D04C9867F8E55}" {
     Id "LF-Hideout_MembersOnly"
     Target_en_us "Members only (%1)"
     Comment "Prompt on a locked hideout door for a non-member; %1 is the owning organisation (#934)."
    }
    CustomStringTableItem "{F763318319EA320C}" {
     Id "LF-Hideout_MembersOnlyPlain"
     Target_en_us "Members only"
     Comment "Prompt on a locked hideout door when the owner name has not reached the client yet (#934)."
    }
    CustomStringTableItem "{1B66611A4CED0EC1}" {
     Id "LF-Hideout_MembersOnlyReason"
     Target_en_us "Only members of %1 can open this door."
     Comment "Refusal under the hideout door prompt; %1 is the owning organisation (#934)."
    }
    CustomStringTableItem "{A9A2726808B24C57}" {
     Id "LF-Hideout_Open"
     Target_en_us "Open"
     Comment "Prompt on a locked hideout door for a member of the owning organisation (#934)."
    }
    CustomStringTableItem "{2C929FC1D06CD839}" {
     Id "LF-Hideout_Unowned"
     Target_en_us "Locked (no owner)"
     Comment "Prompt on a hideout door nobody holds (#934)."
    }
    CustomStringTableItem "{C252650C7A89CED7}" {
     Id "LF-Hideout_UnownedReason"
     Target_en_us "Nobody holds this yard. Found an organization and take the lease at the registry desk."
     Comment "Refusal under the hideout door prompt when no organisation holds the yard (#934)."
    }
    CustomStringTableItem "{365677AB07466EB7}" {
     Id "LF-Hive_Empty"
     Target_en_us "The hive has no comb to take yet."
     Comment "Toast: the beehive is empty (#3447)."
    }
    CustomStringTableItem "{5D9EA11919FB2F2E}" {
     Id "LF-Hive_Take"
     Target_en_us "Take honeycomb (%1)"
     Comment "Action at a beehive with combs; %1 is how many combs it holds (#3447)."
    }
    CustomStringTableItem "{51B7F3A9C8D20E64}" {
     Id "LF-HockeyMask_Description"
     Target_en_us "A yellowed, cracked fiberglass hockey mask with eye slits and drilled vent holes. Anyone wearing it can't be recognized by their face."
     Comment "Headgear item description (tools/assets/hockey-mask)"
    }
    CustomStringTableItem "{C60A94E7D1F3B825}" {
     Id "LF-HockeyMask_Name"
     Target_en_us "Cracked Hockey Mask"
     Comment "Headgear item name (tools/assets/hockey-mask)"
    }
    CustomStringTableItem "{365AAC156EFDBFE8}" {
     Id "LF-HoldUp_Action"
     Target_en_us "Hold up"
     Comment "Interaction menu entry: raise a hand and call hands up on the person in front of you (needs a weapon in hand)."
    }
    CustomStringTableItem "{7CD1C4B0BC457BD5}" {
     Id "LF-HoldUp_Cooldown"
     Target_en_us "Not again so soon."
     Comment "Toast when a hold-up is refused by the cooldown."
    }
    CustomStringTableItem "{4218A5DF1E9BB329}" {
     Id "LF-HoldUp_HeldUp"
     Target_en_us "Someone is holding you up. Talk to them."
     Comment "Toast to the player who was held up."
    }
    CustomStringTableItem "{87043105414D2D3D}" {
     Id "LF-HoldUp_NoWeapon"
     Target_en_us "Take a weapon in hand first."
     Comment "Toast when a hold-up is refused because no weapon is in hand."
    }
    CustomStringTableItem "{39C9CE4F50A7A87A}" {
     Id "LF-HoldUp_Title"
     Target_en_us "Hold up"
     Comment "Toast title for a hold-up."
    }
    CustomStringTableItem "{6DB0CF1985285685}" {
     Id "LF-HoldUp_TooFar"
     Target_en_us "Too far away to hold them up."
     Comment "Toast when a hold-up is refused by range."
    }
    CustomStringTableItem "{C7E29B03FDF73DA5}" {
     Id "LF-HospitalPresence_TooFew"
     Target_en_us "The cabinet stays shut unless a {medic} or two {police} are on duty."
     Comment "Pharmacy cabinet refusal (Feature/Crime/LF_HospitalPresence.c): the robbery needs one medic or two police on duty (pack config numbers)."
    }
    CustomStringTableItem "{DB6C9E5F09D39D13}" {
     Id "LF-Hospital_BillAmount"
     Target_en_us "Treatment: %1"
     Comment "Hospital bill line. %1 is the amount charged."
    }
    CustomStringTableItem "{4F2335135A4FF34F}" {
     Id "LF-Hospital_BillBalance"
     Target_en_us "Bank balance after payment: %1"
     Comment "Hospital bill line. %1 is the bank balance after the bill."
    }
    CustomStringTableItem "{D66A5204D011B83B}" {
     Id "LF-Hospital_BillClose"
     Target_en_us "Put the bill away"
     Comment "Closes the hospital bill card."
    }
    CustomStringTableItem "{1CE638BD33BB8722}" {
     Id "LF-Hospital_BillDebt"
     Target_en_us "You owe the bank. Your next deposits pay the debt first."
     Comment "Hospital bill line shown when the bill put the player into debt."
    }
    CustomStringTableItem "{2DD26F1F4430D002}" {
     Id "LF-Hospital_BillHowToPay"
     Target_en_us "Pay it in cash at the hospital desk."
     Comment "Treatment bill line while unpaid."
    }
    CustomStringTableItem "{BF9E5680A14113DA}" {
     Id "LF-Hospital_BillItem"
     Target_en_us "Hospital bill"
     Comment "Inventory name of the paper hospital bill."
    }
    CustomStringTableItem "{B496BB68DF603A03}" {
     Id "LF-Hospital_BillItemDesc"
     Target_en_us "A treatment bill. Pay it in cash at the hospital desk. Drop it to read it again."
     Comment "Inventory description of the paper hospital bill."
    }
    CustomStringTableItem "{79857CD181EE145E}" {
     Id "LF-Hospital_BillMedic"
     Target_en_us "Treated by: %1"
     Comment "Treatment bill line. %1 is the medic name."
    }
    CustomStringTableItem "{FFEA0B7D7B19622C}" {
     Id "LF-Hospital_BillNewLife"
     Target_en_us "New Life Rule: stay away from where you died for %1 minutes."
     Comment "Hospital bill line: the server rule. %1 is the number of minutes."
    }
    CustomStringTableItem "{94693CF4600BC9D5}" {
     Id "LF-Hospital_BillNote"
     Target_en_us "You don't remember how you got here."
     Comment "Hospital bill note: the New Life Rule cue. The player forgets the fight that killed them."
    }
    CustomStringTableItem "{4956FE4168BECF83}" {
     Id "LF-Hospital_BillNumber"
     Target_en_us "Bill No. %1, %2"
     Comment "Treatment bill line. %1 is the bill number, %2 the in-game date."
    }
    CustomStringTableItem "{008F2EA884F6D7D4}" {
     Id "LF-Hospital_BillPaidLine"
     Target_en_us "Paid in cash on %1. Thank you."
     Comment "Treatment bill line once paid. %1 is the in-game date."
    }
    CustomStringTableItem "{8AA134FCD11961A4}" {
     Id "LF-Hospital_BillRead"
     Target_en_us "Read hospital bill"
     Comment "Action on a hospital bill lying in the world."
    }
    CustomStringTableItem "{25C8757AA18DD8A8}" {
     Id "LF-Hospital_BillTitle"
     Target_en_us "Hospital bill"
     Comment "Title of the paper hospital bill."
    }
    CustomStringTableItem "{BE26E458ADA922A8}" {
     Id "LF-Hospital_BillToast"
     Target_en_us "You woke up in hospital with a treatment bill of %1. Pay it in cash at the hospital desk."
     Comment "Toast on waking in hospital after a death. %1 is the bill amount."
    }
    CustomStringTableItem "{682DF358EBADD27A}" {
     Id "LF-Hospital_Title"
     Target_en_us "Hospital"
     Comment "Toast title when a player wakes up in hospital."
    }
    CustomStringTableItem "{7A3E91C4D5B20F68}" {
     Id "LF-HostSay_Message"
     Target_en_us "%1"
     Comment "Toast body for an operator message sent with tools cli host say; %1 is the operator's text."
    }
    CustomStringTableItem "{2D84B6E0A91C5F37}" {
     Id "LF-HostSay_Title"
     Target_en_us "Message from the server"
     Comment "Toast title for an operator message sent with tools cli host say."
    }
    CustomStringTableItem "{106F4441F054E715}" {
     Id "LF-Hotwire_Action"
     Target_en_us "Hotwire the ignition"
     Comment "Action: hotwire a car that needs its key (#412)."
    }
    CustomStringTableItem "{E9206E012E9770BE}" {
     Id "LF-Hotwire_Already"
     Target_en_us "The ignition is already wired for you"
     Comment "Refusal: already hotwired (#412)."
    }
    CustomStringTableItem "{45ACBB5024E89EBF}" {
     Id "LF-Hotwire_Busy"
     Target_en_us "Someone is already working the ignition"
     Comment "Refusal: an attempt is running (#412)."
    }
    CustomStringTableItem "{D1975826211C1732}" {
     Id "LF-Hotwire_Cancelled"
     Target_en_us "You left the wheel"
     Comment "Toast: the attempt was dropped (#412)."
    }
    CustomStringTableItem "{AADCCA37644F7C29}" {
     Id "LF-Hotwire_Done"
     Target_en_us "The ignition is live. Start the engine."
     Comment "Toast: the hotwire worked (#412)."
    }
    CustomStringTableItem "{65ED248DDAEB762A}" {
     Id "LF-Hotwire_Failed"
     Target_en_us "The wires spark and nothing catches. Try again."
     Comment "Toast: the hotwire failed (#412)."
    }
    CustomStringTableItem "{1C73AA55637B8F93}" {
     Id "LF-Hotwire_HasKey"
     Target_en_us "You have the key"
     Comment "Refusal: the driver holds the key (#412)."
    }
    CustomStringTableItem "{AF65629491E7BFF1}" {
     Id "LF-Hotwire_NoKey"
     Target_en_us "The engine will not start without the key"
     Comment "Toast: engine start refused (#412)."
    }
    CustomStringTableItem "{B7A758401B1C4871}" {
     Id "LF-Hotwire_NotNeeded"
     Target_en_us "This ignition needs no hotwire"
     Comment "Refusal: the car starts freely (#412)."
    }
    CustomStringTableItem "{96CECE0C807F3280}" {
     Id "LF-Hotwire_NotSeated"
     Target_en_us "Sit in the driver seat first"
     Comment "Refusal: not at the wheel (#412)."
    }
    CustomStringTableItem "{B06175474D8ABDD4}" {
     Id "LF-Hotwire_Started"
     Target_en_us "You get under the dash. Stay in the seat."
     Comment "Toast: a hotwire began (#412)."
    }
    CustomStringTableItem "{5212AF1650F40FE3}" {
     Id "LF-Hotwire_Title"
     Target_en_us "Hotwire"
     Comment "Toast title for hotwiring (#412)."
    }
    CustomStringTableItem "{747E2492D28BBC6E}" {
     Id "LF-HoursPlate_Open"
     Target_en_us "OPEN"
     Comment "Hours plate row label on a shop door; the value is the keeper's hours written from the resident timetable."
    }
    CustomStringTableItem "{7B1E4C90A2D35F68}" {
     Id "LF-HouseLock_Lock"
     Target_en_us "Lock house"
     Comment "House door action: the owner or a key holder locks the house"
    }
    CustomStringTableItem "{9D52A7E31C084B6F}" {
     Id "LF-HouseLock_Unlock"
     Target_en_us "Unlock house"
     Comment "House door action: the owner or a key holder unlocks the house"
    }
    CustomStringTableItem "{532DF3B04DBAC43C}" {
     Id "LF-Hud_Bank"
     Target_en_us "Bank %1"
     Comment "HUD: the bank balance under the cash. %1 = amount, e.g. $18,900."
    }
    CustomStringTableItem "{D1557FBF0A771ACC}" {
     Id "LF-Hud_BrandWord"
     Target_en_us "LIFE FRAMEWORK"
     Comment "HUD: the small brand mark top-left. Kept short, upper case."
    }
    CustomStringTableItem "{ADC58C75A825A803}" {
     Id "LF-Hud_CommandDone"
     Target_en_us "HUD: %1"
     Comment "Chat reply after /hud. %1 = current parts, e.g. brand off, money on."
    }
    CustomStringTableItem "{9945A1F5FD9F4B3C}" {
     Id "LF-Hud_CommandUsage"
     Target_en_us "Usage: /hud <brand|money|wanted|needs|all> <on|off>. Now: %1"
     Comment "Chat reply to a wrong /hud command. %1 = current parts, e.g. brand on, money on."
    }
    CustomStringTableItem "{4D79E915332DE722}" {
     Id "LF-Hud_Drink"
     Target_en_us "Drink"
     Comment "HUD: label before the drink number at the bottom of the screen."
    }
    CustomStringTableItem "{E2C3D891635ADE21}" {
     Id "LF-Hud_Food"
     Target_en_us "Food"
     Comment "HUD: label before the food number at the bottom of the screen."
    }
    CustomStringTableItem "{6B84F81BC7DE9FB2}" {
     Id "LF-Hud_Objective"
     Target_en_us "Next: %1"
     Comment "HUD objective line under the money: the step the waypoint follows. %1 = the step title."
    }
    CustomStringTableItem "{3F5C4F04B95FDD98}" {
     Id "LF-Hud_WantedCaption"
     Target_en_us "Wanted"
     Comment "HUD: brief caption beside the wanted badges when the level rises."
    }
    CustomStringTableItem "{57AECD3805F0082C}" {
     Id "LF-ID_CardAge"
     Target_en_us "Age: %1"
     Comment "ID card view age line. %1 is the holder's age."
    }
    CustomStringTableItem "{8864012F52FEEA31}" {
     Id "LF-ID_CardClose"
     Target_en_us "Hand it back"
     Comment "Closes the ID card view."
    }
    CustomStringTableItem "{E0F52B7C093E0EB8}" {
     Id "LF-ID_CardTitle"
     Target_en_us "Identity Card"
     Comment "Title of the ID card view another player holds up."
    }
    CustomStringTableItem "{E071A012FB179790}" {
     Id "LF-ID_NoCard"
     Target_en_us "You are not carrying an ID card."
     Comment "Show ID refused: the player carries no printed ID card."
    }
    CustomStringTableItem "{BB194A05F21678B1}" {
     Id "LF-ID_Shown"
     Target_en_us "You hold up your ID card."
     Comment "Show ID performed: the other player now sees the card."
    }
    CustomStringTableItem "{8974AB0F3FDA7050}" {
     Id "LF-IdCard_Age"
     Target_en_us "AGE"
     Comment "Identity card sheet, label over the holder's age (#902)."
    }
    CustomStringTableItem "{71C47D0D3468E849}" {
     Id "LF-IdCard_ClerkNeverIssued"
     Target_en_us "You are not in the register yet. Tell me you are new in {town}."
     Comment "Clerk's answer: the player was never issued a card (#902)."
    }
    CustomStringTableItem "{180AD6EEE0179AC7}" {
     Id "LF-IdCard_ClerkNoCash"
     Target_en_us "A new card costs the fee, in cash. Come back when you have it."
     Comment "Clerk's answer: not enough carried cash for the replacement fee (#902)."
    }
    CustomStringTableItem "{547D1993FB0A4B30}" {
     Id "LF-IdCard_ClerkReplaced"
     Target_en_us "Here is your new card, with a new photograph. The old one is void now."
     Comment "Clerk's answer after printing a replacement card (#902). Shown as a caption."
    }
    CustomStringTableItem "{D8E1C5B11C9D2A03}" {
     Id "LF-IdCard_ClerkStillCarried"
     Target_en_us "You have your card with you. I cannot issue a second one."
     Comment "Clerk's answer: the player still carries their valid card (#902)."
    }
    CustomStringTableItem "{5D9A5929885AD937}" {
     Id "LF-IdCard_FeeToast"
     Target_en_us "A replacement card costs %1 in cash."
     Comment "Toast when the player cannot pay the replacement fee. %1 is the fee (#902)."
    }
    CustomStringTableItem "{D2242585051FB370}" {
     Id "LF-IdCard_GivenName"
     Target_en_us "GIVEN NAMES"
     Comment "Identity card sheet, label over the holder's first name (#902)."
    }
    CustomStringTableItem "{9068B20753AD311C}" {
     Id "LF-IdCard_Issued"
     Target_en_us "DATE OF ISSUE"
     Comment "Identity card sheet, label over the in-game date the card was printed (#902)."
    }
    CustomStringTableItem "{591F38DE83F87F4C}" {
     Id "LF-IdCard_ItemDesc"
     Target_en_us "A laminated identity card from the civil registry. Inspect it to read it."
     Comment "Inventory description of the identity card item (#902)."
    }
    CustomStringTableItem "{3D747C82D484541D}" {
     Id "LF-IdCard_ItemName"
     Target_en_us "Identity card"
     Comment "Inventory name of the identity card item (#902)."
    }
    CustomStringTableItem "{C7EC358FCA8548A2}" {
     Id "LF-IdCard_Number"
     Target_en_us "CARD NO."
     Comment "Identity card sheet, label over the card number (#902)."
    }
    CustomStringTableItem "{891051F242392738}" {
     Id "LF-IdCard_PutAway"
     Target_en_us "Put it away"
     Comment "Closes your own identity card sheet (inventory Inspect, #902)."
    }
    CustomStringTableItem "{1252EACF4F5E22BC}" {
     Id "LF-IdCard_Registry"
     Target_en_us "CIVIL REGISTRY  -  FORM R-89"
     Comment "Identity card sheet, the issuing office under the title (#902)."
    }
    CustomStringTableItem "{2BF7801852A79CB9}" {
     Id "LF-IdCard_ReplacedToast"
     Target_en_us "New identity card issued for %1. Your old card is void."
     Comment "Toast after a replacement card. %1 is the fee paid (#902)."
    }
    CustomStringTableItem "{AB210173D6113649}" {
     Id "LF-IdCard_Request"
     Target_en_us "Request ID"
     Comment "Player interaction menu entry for an on-duty officer: ask the person for their identity card (#902)."
    }
    CustomStringTableItem "{447080BA06948A97}" {
     Id "LF-IdCard_RequestBody"
     Target_en_us "A {police} officer asks to see your identity card."
     Comment "Body of the ID check sheet (#902)."
    }
    CustomStringTableItem "{B2CC8FC403E5B834}" {
     Id "LF-IdCard_RequestNoCard"
     Target_en_us "They are willing, but cannot show you a card."
     Comment "Toast to the officer: the citizen agreed to show their card but carries none, or walked out of reach (#902)."
    }
    CustomStringTableItem "{E3DEB3917A55F4DC}" {
     Id "LF-IdCard_RequestRefuse"
     Target_en_us "Refuse"
     Comment "ID check sheet button: refuse to show the card (#902)."
    }
    CustomStringTableItem "{62D648A2CF6FBFB3}" {
     Id "LF-IdCard_RequestRefused"
     Target_en_us "They refuse to show their identity card."
     Comment "Toast to the officer when the citizen refuses (#902)."
    }
    CustomStringTableItem "{5D662C6CCD9417A1}" {
     Id "LF-IdCard_RequestSent"
     Target_en_us "You ask to see their identity card."
     Comment "Toast to the officer after Request ID (#902)."
    }
    CustomStringTableItem "{6E11F94962BCC683}" {
     Id "LF-IdCard_RequestShow"
     Target_en_us "Show my card"
     Comment "ID check sheet button: hold the card up to the officer (#902)."
    }
    CustomStringTableItem "{F33F94AF0E77E59B}" {
     Id "LF-IdCard_RequestTitle"
     Target_en_us "ID CHECK"
     Comment "Title of the sheet on the citizen's screen when an officer asks for their card (#902)."
    }
    CustomStringTableItem "{A04900D7D79EB946}" {
     Id "LF-IdCard_RequestWaiting"
     Target_en_us "You have already asked. Give them a moment."
     Comment "Toast to an officer who asks again while the citizen's ID check is still open (#902)."
    }
    CustomStringTableItem "{BF0E296814553E0D}" {
     Id "LF-IdCard_Surname"
     Target_en_us "SURNAME"
     Comment "Identity card sheet, label over the holder's last name (#902)."
    }
    CustomStringTableItem "{FEFDDA5887179920}" {
     Id "LF-IdCard_Void"
     Target_en_us "VOID - REPLACED"
     Comment "Identity card sheet: this card was replaced at the clerk and is no longer valid (#902)."
    }
    CustomStringTableItem "{7B00DCF639B7E3D1}" {
     Id "LF-IdCard_VoidLabel"
     Target_en_us "STATUS"
     Comment "Identity card sheet, label of the line shown on a replaced card (#902)."
    }
    CustomStringTableItem "{1736103CC4077F8B}" {
     Id "LF-Impound_CannotAfford"
     Target_en_us "The release fee is $%1 in cash."
     Comment "Impound collect refused: carried cash is short. %1 is the fee"
    }
    CustomStringTableItem "{1A7DF5F007730F80}" {
     Id "LF-Impound_Closed"
     Target_en_us "The impound desk is closed."
     Comment "Impound collect refused: the fee is not configured"
    }
    CustomStringTableItem "{7E2E61FB397A0CDF}" {
     Id "LF-Impound_Collect"
     Target_en_us "Pay the fee and collect your vehicle"
     Comment "Impound desk action"
    }
    CustomStringTableItem "{E4FB9D76BA99F73A}" {
     Id "LF-Impound_Collected"
     Target_en_us "Released: %1 is waiting in the yard."
     Comment "Impound collect succeeded. %1 is the plate"
    }
    CustomStringTableItem "{8A114E81A1FA0278}" {
     Id "LF-Impound_NothingHere"
     Target_en_us "None of your vehicles is at the impound lot."
     Comment "Impound collect refused: nothing impounded for this player"
    }
    CustomStringTableItem "{DF52697EEFAFA6B6}" {
     Id "LF-Impound_PadBusy"
     Target_en_us "The yard is full. Wait until a vehicle is driven out."
     Comment "Impound collect refused: every yard pad is occupied"
    }
    CustomStringTableItem "{50E0662C55792133}" {
     Id "LF-Impound_Title"
     Target_en_us "Impound Lot"
     Comment "Notification title at the impound desk"
    }
    CustomStringTableItem "{9B2D4E60A17C35F8}" {
     Id "LF-IncidentGm_Kind_admin"
     Target_en_us "Admin action"
     Comment "Incident kind: an administrator or Game Master did something (teleport, heal, kick, whitelist...)."
    }
    CustomStringTableItem "{3C5A9E71D04B82F6}" {
     Id "LF-IncidentGm_Kind_aim"
     Target_en_us "Weapon aimed"
     Comment "Incident kind: one player kept a raised weapon on another."
    }
    CustomStringTableItem "{608D778B3672F032}" {
     Id "LF-IncidentGm_Kind_cash"
     Target_en_us "Cash"
     Comment "Incident kind: cash move."
    }
    CustomStringTableItem "{EC2EA6F457CE29F4}" {
     Id "LF-IncidentGm_Kind_damage"
     Target_en_us "Damage"
     Comment "Incident kind: damage."
    }
    CustomStringTableItem "{3425A63D351F8477}" {
     Id "LF-IncidentGm_Kind_death"
     Target_en_us "Death"
     Comment "Incident kind: death."
    }
    CustomStringTableItem "{2CBEE27A4CEA58CE}" {
     Id "LF-IncidentGm_Kind_downed"
     Target_en_us "Downed"
     Comment "Incident kind: player downed."
    }
    CustomStringTableItem "{F8678369812B752F}" {
     Id "LF-IncidentGm_Kind_holdup"
     Target_en_us "Hold-up"
     Comment "Incident kind: one player held another up."
    }
    CustomStringTableItem "{46261105E82813B6}" {
     Id "LF-IncidentGm_Kind_logout"
     Target_en_us "Logout in custody"
     Comment "Incident kind: a player logged out while cuffed, escorted, downed, wanted or jailed; the row adds the state and what the disconnect rules did."
    }
    CustomStringTableItem "{B96B4DA1873C79D6}" {
     Id "LF-IncidentGm_Kind_murder"
     Target_en_us "Murder"
     Comment "Incident kind: death counted as murder."
    }
    CustomStringTableItem "{4E7FF02882373B33}" {
     Id "LF-IncidentGm_Kind_nlr"
     Target_en_us "New Life Rule return"
     Comment "Incident kind: a player wearing the New Life wristband came back near where they died."
    }
    CustomStringTableItem "{0532BB8A674B16E7}" {
     Id "LF-IncidentGm_None"
     Target_en_us "No incidents here in the last hour."
     Comment "Game Master incident panel: nothing near the camera."
    }
    CustomStringTableItem "{D4CB8C3C4F736D4A}" {
     Id "LF-IncidentGm_Row"
     Target_en_us "%1, %2 min ago: %3 / %4"
     Comment "Game Master incident row: kind, minutes ago, subject, other party."
    }
    CustomStringTableItem "{C8483035D57F363E}" {
     Id "LF-IncidentGm_RowFlagged"
     Target_en_us "REPORTED #%1 %2, %3 min ago: %4 / %5 (type !handled %1 when done)"
     Comment "Game Master incident row for a reported incident: number, kind, minutes ago, subject, other party."
    }
    CustomStringTableItem "{98E8442712C32CFF}" {
     Id "LF-IncidentGm_Title"
     Target_en_us "Incidents near the camera"
     Comment "Game Master incident panel title."
    }
    CustomStringTableItem "{575E08BEF8CB275E}" {
     Id "LF-IncidentGm_Waiting"
     Target_en_us "Asking the server..."
     Comment "Game Master incident panel: before the first answer."
    }
    CustomStringTableItem "{8C5397BA37749A31}" {
     Id "LF-IncidentReport_Cooldown"
     Target_en_us "You reported a moment ago. Wait before reporting again."
     Comment "Toast: the report cooldown refused a second report."
    }
    CustomStringTableItem "{00E362FF96E1289A}" {
     Id "LF-IncidentReport_Nothing"
     Target_en_us "Nothing of yours to report from the last few minutes."
     Comment "Toast: the player has no recent incident."
    }
    CustomStringTableItem "{C4AEA0F427A28843}" {
     Id "LF-IncidentReport_Sent"
     Target_en_us "Staff were told about your recent incidents."
     Comment "Toast: a player report was filed."
    }
    CustomStringTableItem "{3A18F927506DDB4E}" {
     Id "LF-IncidentReport_Staff"
     Target_en_us "A player reported %1 incident(s). They are first in the Game Master incident list."
     Comment "Toast to online admins after a report; %1 is the count."
    }
    CustomStringTableItem "{73C1578BB2FBEF30}" {
     Id "LF-IncidentReport_Title"
     Target_en_us "Report"
     Comment "Toast title for the report to staff."
    }
    CustomStringTableItem "{A2F0BAF8FDA44A7C}" {
     Id "LF-Informant_Already"
     Target_en_us "I told you already. He is where I marked it."
     Comment "Informant answer (#2921): the player already holds this period's mark; nothing was taken."
    }
    CustomStringTableItem "{B96A99BF8300C7A3}" {
     Id "LF-Informant_Mark"
     Target_en_us "The buyer"
     Comment "Map mark label (#2921): where the informant says the illicit buyer is standing today; private to the player who paid."
    }
    CustomStringTableItem "{8BD615448C30E422}" {
     Id "LF-Informant_NeverHeard"
     Target_en_us "Never heard of him."
     Comment "Informant refusal (#2921): police on duty, or too little underworld standing; the same words either way. Also the toast."
    }
    CustomStringTableItem "{239CCFD8A1E56524}" {
     Id "LF-Informant_NoCash"
     Target_en_us "The tip costs {currency:sym}%1. Come back when you have it."
     Comment "Informant refusal toast (#2921): not enough cash; %1 is the fee. Nothing was taken."
    }
    CustomStringTableItem "{0BFA75DDB569A395}" {
     Id "LF-Informant_NoSpot"
     Target_en_us "Nobody is buying today. Nothing to sell you."
     Comment "Informant answer (#2921): no buyer spot is placed on this map; nothing was taken."
    }
    CustomStringTableItem "{CECFA4732D210AB0}" {
     Id "LF-Informant_Told"
     Target_en_us "Heard he is buying today. I marked it on your map; the spot will not hold past this shift."
     Comment "Informant answer (#2921): the fee was paid and the buyer's spot is on the map; also the toast."
    }
    CustomStringTableItem "{B1A4C6D2E8F03955}" {
     Id "LF-Inspection_CannotInspect"
     Target_en_us "This car cannot be inspected right now."
     Comment "Inspection toast (#3433): the car has no id or the world has no clock."
    }
    CustomStringTableItem "{B1A4C6D2E8F03952}" {
     Id "LF-Inspection_Check"
     Target_en_us "Check the sticker"
     Comment "Action on the inspection desk (#3433): read the sticker of the car beside the desk."
    }
    CustomStringTableItem "{B1A4C6D2E8F03958}" {
     Id "LF-Inspection_Expired"
     Target_en_us "The sticker has expired."
     Comment "Sticker check result (#3433): inspected, expiry day passed."
    }
    CustomStringTableItem "{B1A4C6D2E8F03951}" {
     Id "LF-Inspection_Inspect"
     Target_en_us "Inspect vehicle"
     Comment "Action on the inspection desk (#3433): stamp the car beside the desk with a sticker."
    }
    CustomStringTableItem "{B1A4C6D2E8F03954}" {
     Id "LF-Inspection_NoCar"
     Target_en_us "No car beside the desk."
     Comment "Inspection toast (#3433): no vehicle within reach."
    }
    CustomStringTableItem "{B1A4C6D2E8F03959}" {
     Id "LF-Inspection_None"
     Target_en_us "This car has no inspection sticker."
     Comment "Sticker check result (#3433): never inspected."
    }
    CustomStringTableItem "{B1A4C6D2E8F03953}" {
     Id "LF-Inspection_OnDutyOnly"
     Target_en_us "Only an on-duty officer can do this."
     Comment "Inspection action refused (#3433): the user is not on-duty police."
    }
    CustomStringTableItem "{B1A4C6D2E8F03956}" {
     Id "LF-Inspection_Stamped"
     Target_en_us "Inspection passed. The sticker is valid for %1 days."
     Comment "Inspection toast (#3433). %1 = the sticker's validity in game days."
    }
    CustomStringTableItem "{B1A4C6D2E8F03957}" {
     Id "LF-Inspection_Valid"
     Target_en_us "The sticker is valid."
     Comment "Sticker check result (#3433): inspected and not yet expired."
    }
    CustomStringTableItem "{00B6F39DAD843E76}" {
     Id "LF-Interact_AlreadyKnown"
     Target_en_us "They already know your name."
     Comment "Introduce: the other player had already been introduced to you."
    }
    CustomStringTableItem "{35BF5A30C7E8CB61}" {
     Id "LF-Interact_BadAmount"
     Target_en_us "Write a whole amount above zero."
     Comment "Give cash sheet: the amount is not a positive whole number."
    }
    CustomStringTableItem "{A8E16EB722360B68}" {
     Id "LF-Interact_CashFailed"
     Target_en_us "The cash did not change hands."
     Comment "Give cash failed: the receiver could not take it; nothing moved."
    }
    CustomStringTableItem "{F3146FBEA250635F}" {
     Id "LF-Interact_CashGiven"
     Target_en_us "You handed over {currency:sym}%1."
     Comment "Give cash performed, giver side. %1 is the amount."
    }
    CustomStringTableItem "{CDE85A79D1DDEB85}" {
     Id "LF-Interact_CashReceived"
     Target_en_us "You were handed {currency:sym}%1."
     Comment "Give cash performed, receiver side. %1 is the amount."
    }
    CustomStringTableItem "{7E14502EF85F5A4A}" {
     Id "LF-Interact_GiveCash"
     Target_en_us "Give cash"
     Comment "Player interaction: hand carried cash to the person in front of you."
    }
    CustomStringTableItem "{D123351114EF634A}" {
     Id "LF-Interact_GiveCashAmount"
     Target_en_us "Amount"
     Comment "Give cash sheet field label."
    }
    CustomStringTableItem "{2E7513CA7BB7DDF1}" {
     Id "LF-Interact_GiveCashButton"
     Target_en_us "Hand it over"
     Comment "Give cash sheet button that sends the amount."
    }
    CustomStringTableItem "{C316763AB24BFEC1}" {
     Id "LF-Interact_GiveCashCarried"
     Target_en_us "In your pocket: {currency:sym}%1"
     Comment "Give cash sheet line. %1 is the cash carried."
    }
    CustomStringTableItem "{BDC4A6899C77AE87}" {
     Id "LF-Interact_GiveCashKeep"
     Target_en_us "Keep it"
     Comment "Give cash sheet close button."
    }
    CustomStringTableItem "{71D81EDC67E0708A}" {
     Id "LF-Interact_GiveCashTitle"
     Target_en_us "Hand over cash"
     Comment "Title of the give cash sheet."
    }
    CustomStringTableItem "{02C4E3CA42A96CE2}" {
     Id "LF-Interact_GiveItem"
     Target_en_us "Give what you hold"
     Comment "Player interaction: hand the item in your hands to the person in front of you."
    }
    CustomStringTableItem "{8E626C1D03166F0E}" {
     Id "LF-Interact_Introduce"
     Target_en_us "Introduce yourself"
     Comment "Player interaction: tell the person in front of you your name."
    }
    CustomStringTableItem "{892ED800F24AFCCA}" {
     Id "LF-Interact_Introduced"
     Target_en_us "You told them your name."
     Comment "Introduce performed, shown to the one introducing themselves."
    }
    CustomStringTableItem "{D291A0929FB8ADB3}" {
     Id "LF-Interact_ItemGiven"
     Target_en_us "You handed it over."
     Comment "Give item performed, giver side."
    }
    CustomStringTableItem "{9868061D4DF3FF33}" {
     Id "LF-Interact_ItemReceived"
     Target_en_us "You were handed something."
     Comment "Give item performed, receiver side."
    }
    CustomStringTableItem "{85B3688798684AE5}" {
     Id "LF-Interact_MetSomeone"
     Target_en_us "%1 introduced themselves."
     Comment "Introduce received. %1 is the other player's character name."
    }
    CustomStringTableItem "{1ECCEC67D6536926}" {
     Id "LF-Interact_NoCash"
     Target_en_us "You are not carrying any cash."
     Comment "Give cash refused: nothing carried."
    }
    CustomStringTableItem "{962C35F4A89E8FB6}" {
     Id "LF-Interact_NoOne"
     Target_en_us "There is no one to talk to."
     Comment "Player interaction refused: no player in front of you."
    }
    CustomStringTableItem "{9DCD0FDDC5828103}" {
     Id "LF-Interact_NoRoom"
     Target_en_us "They have no room for it."
     Comment "Give item refused: the receiver's inventory has no space."
    }
    CustomStringTableItem "{94040D66A82501E4}" {
     Id "LF-Interact_NotAlive"
     Target_en_us "They cannot respond."
     Comment "Player interaction refused: one of the two is dead."
    }
    CustomStringTableItem "{C5A7B4999DC6BE9B}" {
     Id "LF-Interact_NotEnoughCash"
     Target_en_us "You do not carry that much."
     Comment "Give cash refused: amount above the cash carried."
    }
    CustomStringTableItem "{4785856902D6478D}" {
     Id "LF-Interact_NothingInHand"
     Target_en_us "You are not holding anything."
     Comment "Give item refused: nothing in hand."
    }
    CustomStringTableItem "{A671E78C5ACEFC66}" {
     Id "LF-Interact_RoleSlot"
     Target_en_us "Role action"
     Comment "Fallback name of a role slot (fine, org invite); the registered handler names it."
    }
    CustomStringTableItem "{60B9F032C5F7B335}" {
     Id "LF-Interact_Title"
     Target_en_us "Person to person"
     Comment "Toast title for the player interaction menu."
    }
    CustomStringTableItem "{8CADCC5825E64F26}" {
     Id "LF-Interact_TooFar"
     Target_en_us "Step closer to them."
     Comment "Player interaction refused: the other person is out of reach."
    }
    CustomStringTableItem "{218E14C1F987A256}" {
     Id "LF-Inventory_DroppedAtFeet"
     Target_en_us "No room on you: %1 x%2 is on the ground at your feet."
     Comment "Toast when a given item (a press output, a catch, a harvest) did not fit the inventory and was dropped beside the player. %1 = item name, %2 = amount"
    }
    CustomStringTableItem "{7C1E4A92B05D3F18}" {
     Id "LF-ItemName_HouseKeyOf"
     Target_en_us "House Key [%1]"
     Comment "Inventory name of a house key bound to a house. %1 is the house identifier it opens (#2470)."
    }
    CustomStringTableItem "{2D9B60E7A4C85F31}" {
     Id "LF-ItemName_VehicleKeyOf"
     Target_en_us "Vehicle Key [%1]"
     Comment "Inventory name of a vehicle key bound to a vehicle. %1 is the vehicle identifier it opens (#2470)."
    }
    CustomStringTableItem "{2DC6A819C67F6815}" {
     Id "LF-Item_AcidCarboy_Description"
     Target_en_us "A carboy of acid for the copper refinery. Heavy and not for drinking."
     Comment "Item description of the acid carboy (mining chains, #3230)."
    }
    CustomStringTableItem "{CEA9A2F55DD1E2F5}" {
     Id "LF-Item_AcidCarboy_Name"
     Target_en_us "Acid carboy"
     Comment "Item name of the acid carboy."
    }
    CustomStringTableItem "{106871628C24F2E1}" {
     Id "LF-Item_AspirinBottle_Name"
     Target_en_us "Aspirin"
     Comment "Pharmacy row name of the base-game aspirin bottle, which has no inventory name (#5080)."
    }
    CustomStringTableItem "{AC88D3331A83BBB1}" {
     Id "LF-Item_BaitTin_Description"
     Target_en_us "A green enameled can with a snap lid. Worms and dough balls, mostly."
     Comment "Item description: the bait tin (#2928)."
    }
    CustomStringTableItem "{27D7F2EA4C764F06}" {
     Id "LF-Item_BaitTin_Name"
     Target_en_us "Bait can"
     Comment "Item name: the bait tin (#2928)."
    }
    CustomStringTableItem "{5FAF17A9D2358E1C}" {
     Id "LF-Item_BeehiveKit"
     Target_en_us "Beehive"
     Comment "Item name: a beehive kit that is placed outdoors to keep bees (#3448)."
    }
    CustomStringTableItem "{9BDC6A38FFE77DE8}" {
     Id "LF-Item_BeehiveKitDesc"
     Target_en_us "A beehive to set up outdoors. Bees work it by day in fair weather; take the honeycomb when it is full."
     Comment "Item description: beehive kit (#3448)."
    }
    CustomStringTableItem "{39E8DFFDB0869E08}" {
     Id "LF-Item_CarParts_Description"
     Target_en_us "Spare parts stripped off a stolen car. A trader pays a little for them."
     Comment "Item description (#415)."
    }
    CustomStringTableItem "{306F9E34BC7297E7}" {
     Id "LF-Item_CarParts_Name"
     Target_en_us "Car parts"
     Comment "Item name: spare parts from a chopped car (#415)."
    }
    CustomStringTableItem "{62DA24F968E17906}" {
     Id "LF-Item_Charge_Description"
     Target_en_us "A shaped charge. The serial is stamped on the casing."
     Comment "Item description of the fence charge."
    }
    CustomStringTableItem "{C422F998AB82B4F0}" {
     Id "LF-Item_Charge_Name"
     Target_en_us "Breaching charge"
     Comment "Item name of the fence charge."
    }
    CustomStringTableItem "{7AD691E65BE531F0}" {
     Id "LF-Item_CoalLump_Description"
     Target_en_us "A lump of coal. Smelters burn it to turn iron nuggets into bars."
     Comment "Item description of the coal lump (#3230)."
    }
    CustomStringTableItem "{E90681E4967BF028}" {
     Id "LF-Item_CoalLump_Name"
     Target_en_us "Coal"
     Comment "Item name of the coal lump."
    }
    CustomStringTableItem "{DAB831471E8878CE}" {
     Id "LF-Item_Crackers"
     Target_en_us "Crackers"
     Comment "Item name: a packet of dry crackers (#1143)."
    }
    CustomStringTableItem "{BD823BCC5158CF32}" {
     Id "LF-Item_CrackersDesc"
     Target_en_us "A packet of dry crackers. Keeps for months."
     Comment "Item description: crackers (#1143)."
    }
    CustomStringTableItem "{B94E7A21D5C03F68}" {
     Id "LF-Item_DoorKit_Description"
     Target_en_us "A boxed house door with its hinges and lock. The last stage of a cottage asks for one."
     Comment "Item description (#3172)."
    }
    CustomStringTableItem "{A17C60E9F3B2D845}" {
     Id "LF-Item_DoorKit_Name"
     Target_en_us "Door kit"
     Comment "Item name: boxed door for the cottage's door stage (#3172)."
    }
    CustomStringTableItem "{8D5931803863C15C}" {
     Id "LF-Item_DriedMeat"
     Target_en_us "Dried sausage"
     Comment "Item name: a dried sausage (#1143)."
    }
    CustomStringTableItem "{19716E0DDC033FA9}" {
     Id "LF-Item_DriedMeatDesc"
     Target_en_us "A hard dried sausage. Filling, and it keeps."
     Comment "Item description: dried sausage (#1143)."
    }
    CustomStringTableItem "{39F1DED905C80D73}" {
     Id "LF-Item_Drill_Description"
     Target_en_us "A hardened drill. The serial is stamped under the grip."
     Comment "Item description of the fence drill."
    }
    CustomStringTableItem "{177011F784696AD3}" {
     Id "LF-Item_Drill_Name"
     Target_en_us "Drill"
     Comment "Item name of the fence drill."
    }
    CustomStringTableItem "{B5EBEF52E13CDECE}" {
     Id "LF-Item_EntrenchingTool_Name"
     Target_en_us "Entrenching tool"
     Comment "Shop row name of the folding shovel the hardware store sells (#5080); the base-game prefab has no inventory name."
    }
    CustomStringTableItem "{E2784D1F4353D38F}" {
     Id "LF-Item_FishBass_Description"
     Target_en_us "A sea bass with a spiny back fin."
     Comment "Item description: bass (#3119)."
    }
    CustomStringTableItem "{9A898B30BBECE54A}" {
     Id "LF-Item_FishBass_Large_Description"
     Target_en_us "A big sea bass. A proud catch."
     Comment "Item description: Large bass (#3119)."
    }
    CustomStringTableItem "{9359D41CCCD09C91}" {
     Id "LF-Item_FishBass_Large_Name"
     Target_en_us "Large bass"
     Comment "Item name: Large bass (#3119)."
    }
    CustomStringTableItem "{6B6D20BD67714955}" {
     Id "LF-Item_FishBass_Name"
     Target_en_us "Bass"
     Comment "Item name: bass (#3119)."
    }
    CustomStringTableItem "{3D4C47BC0C448471}" {
     Id "LF-Item_FishBass_Small_Description"
     Target_en_us "A small sea bass. Quick and silvery."
     Comment "Item description: Small bass (#3119)."
    }
    CustomStringTableItem "{D1B389AD889452FD}" {
     Id "LF-Item_FishBass_Small_Name"
     Target_en_us "Small bass"
     Comment "Item name: Small bass (#3119)."
    }
    CustomStringTableItem "{5DB4327C8FD8C6F5}" {
     Id "LF-Item_FishCarp_Description"
     Target_en_us "A carp, broad and golden-brown."
     Comment "Item description: carp (#3119)."
    }
    CustomStringTableItem "{3E331EBEA033DBF8}" {
     Id "LF-Item_FishCarp_Large_Description"
     Target_en_us "A big carp. Slow, heavy and old."
     Comment "Item description: Large carp (#3119)."
    }
    CustomStringTableItem "{00FCE916EC8F207B}" {
     Id "LF-Item_FishCarp_Large_Name"
     Target_en_us "Large carp"
     Comment "Item name: Large carp (#3119)."
    }
    CustomStringTableItem "{E3B3001507AC877B}" {
     Id "LF-Item_FishCarp_Name"
     Target_en_us "Carp"
     Comment "Item name: carp (#3119)."
    }
    CustomStringTableItem "{34A3AD0F6B77EB36}" {
     Id "LF-Item_FishCarp_Small_Description"
     Target_en_us "A small carp, bronze and scaly."
     Comment "Item description: Small carp (#3119)."
    }
    CustomStringTableItem "{38CA8EAD72C90BF2}" {
     Id "LF-Item_FishCarp_Small_Name"
     Target_en_us "Small carp"
     Comment "Item name: Small carp (#3119)."
    }
    CustomStringTableItem "{4B56E52FB9CF207D}" {
     Id "LF-Item_FishMackerel_Description"
     Target_en_us "A good mackerel, firm and streaked."
     Comment "Item description: mackerel (#3119)."
    }
    CustomStringTableItem "{C4DDAEF89A37519D}" {
     Id "LF-Item_FishMackerel_Large_Description"
     Target_en_us "A big mackerel. Heavy for its kind."
     Comment "Item description: Large mackerel (#3119)."
    }
    CustomStringTableItem "{E73AE5B5C5FB7443}" {
     Id "LF-Item_FishMackerel_Large_Name"
     Target_en_us "Large mackerel"
     Comment "Item name: Large mackerel (#3119)."
    }
    CustomStringTableItem "{5E0A92C3AF9164C4}" {
     Id "LF-Item_FishMackerel_Name"
     Target_en_us "Mackerel"
     Comment "Item name: mackerel (#3119)."
    }
    CustomStringTableItem "{69C02856EEC8D522}" {
     Id "LF-Item_FishMackerel_Small_Description"
     Target_en_us "A small mackerel, silver and green-barred."
     Comment "Item description: Small mackerel (#3119)."
    }
    CustomStringTableItem "{633C51C7D45BA350}" {
     Id "LF-Item_FishMackerel_Small_Name"
     Target_en_us "Small mackerel"
     Comment "Item name: Small mackerel (#3119)."
    }
    CustomStringTableItem "{AB96FAED4AECB267}" {
     Id "LF-Item_FishPerch_Description"
     Target_en_us "A perch, green-gold with red fins."
     Comment "Item description: perch (#3119)."
    }
    CustomStringTableItem "{061E58D13F51C5C8}" {
     Id "LF-Item_FishPerch_Large_Description"
     Target_en_us "A big perch. Striped and stout."
     Comment "Item description: Large perch (#3119)."
    }
    CustomStringTableItem "{16B0C23F1964DB20}" {
     Id "LF-Item_FishPerch_Large_Name"
     Target_en_us "Large perch"
     Comment "Item name: Large perch (#3119)."
    }
    CustomStringTableItem "{F57F65FB08C8F73A}" {
     Id "LF-Item_FishPerch_Name"
     Target_en_us "Perch"
     Comment "Item name: perch (#3119)."
    }
    CustomStringTableItem "{A12E910AA22B4796}" {
     Id "LF-Item_FishPerch_Small_Description"
     Target_en_us "A small perch with dark stripes."
     Comment "Item description: Small perch (#3119)."
    }
    CustomStringTableItem "{379C675811B9535B}" {
     Id "LF-Item_FishPerch_Small_Name"
     Target_en_us "Small perch"
     Comment "Item name: Small perch (#3119)."
    }
    CustomStringTableItem "{D9C27E711957F2A2}" {
     Id "LF-Item_FishTrout_Description"
     Target_en_us "A decent brown trout, olive on the back and pale underneath."
     Comment "Item description: the trout base prefab (#2928)."
    }
    CustomStringTableItem "{6BB0D04BABA540C6}" {
     Id "LF-Item_FishTrout_Large_Description"
     Target_en_us "A big brown trout. Worth the wait."
     Comment "Item description: a large trout (#2928)."
    }
    CustomStringTableItem "{0EF8318669237071}" {
     Id "LF-Item_FishTrout_Large_Name"
     Target_en_us "Large trout"
     Comment "Item name: a large trout (#2928)."
    }
    CustomStringTableItem "{2936F3C2F236AEF1}" {
     Id "LF-Item_FishTrout_Name"
     Target_en_us "Trout"
     Comment "Item name: the medium trout, the base prefab (#2928)."
    }
    CustomStringTableItem "{1A89123E3BDB8888}" {
     Id "LF-Item_FishTrout_Small_Description"
     Target_en_us "A small brown trout. Barely a mouthful."
     Comment "Item description: a small trout (#2928)."
    }
    CustomStringTableItem "{0F3CF69DC9F74BE4}" {
     Id "LF-Item_FishTrout_Small_Name"
     Target_en_us "Small trout"
     Comment "Item name: a small trout (#2928)."
    }
    CustomStringTableItem "{860E865EA857C343}" {
     Id "LF-Item_FishTuna_Description"
     Target_en_us "A tuna, solid muscle from nose to tail."
     Comment "Item description: tuna (#3119)."
    }
    CustomStringTableItem "{A9743CC382E8A306}" {
     Id "LF-Item_FishTuna_Large_Description"
     Target_en_us "A huge tuna. It took real work to land."
     Comment "Item description: Large tuna (#3119)."
    }
    CustomStringTableItem "{31EDD97E30F43C62}" {
     Id "LF-Item_FishTuna_Large_Name"
     Target_en_us "Large tuna"
     Comment "Item name: Large tuna (#3119)."
    }
    CustomStringTableItem "{73662CFC234FFF2B}" {
     Id "LF-Item_FishTuna_Name"
     Target_en_us "Tuna"
     Comment "Item name: tuna (#3119)."
    }
    CustomStringTableItem "{39AC76C237D894BF}" {
     Id "LF-Item_FishTuna_Small_Description"
     Target_en_us "A young tuna, torpedo-shaped and dark blue."
     Comment "Item description: Small tuna (#3119)."
    }
    CustomStringTableItem "{55C653E598CCC44F}" {
     Id "LF-Item_FishTuna_Small_Name"
     Target_en_us "Small tuna"
     Comment "Item name: Small tuna (#3119)."
    }
    CustomStringTableItem "{377E1717041FCC31}" {
     Id "LF-Item_FishingRod_Description"
     Target_en_us "A cork-handled fiberglass rod with a fixed-spool reel. Take it into your hands from a quick slot."
     Comment "Item description: the fishing rod (#2928)."
    }
    CustomStringTableItem "{2039700C106A3719}" {
     Id "LF-Item_FishingRod_Name"
     Target_en_us "Fishing rod"
     Comment "Item name: the fishing rod (#2928)."
    }
    CustomStringTableItem "{4D123BBBA06D16BA}" {
     Id "LF-Item_GangRadio_Description"
     Target_en_us "A handheld set. Tune it to your gang channel and only your crew hears you."
     Comment "Item description of the gang radio (#4221)."
    }
    CustomStringTableItem "{3337BFB02E6A4A9F}" {
     Id "LF-Item_GangRadio_Name"
     Target_en_us "Gang radio"
     Comment "Item name: a handheld radio a gang tunes to its own hidden channel (#4221)."
    }
    CustomStringTableItem "{E0C70F7F81117F7D}" {
     Id "LF-Item_Handcuffs_Description"
     Target_en_us "Steel chain handcuffs. Hold them and face a downed or held-up suspect to cuff them."
     Comment "Inventory description of the police handcuffs."
    }
    CustomStringTableItem "{6B316884EEFB27FF}" {
     Id "LF-Item_Handcuffs_Name"
     Target_en_us "Handcuffs"
     Comment "Inventory name of the police handcuffs."
    }
    CustomStringTableItem "{B343B39B5EC2F09D}" {
     Id "LF-Item_HandsetRadio_Description"
     Target_en_us "A handheld set. Calls a cab, a doctor or an officer from wherever you stand, for a small fee."
     Comment "Item description of the radio set. The engine renders a prefab description itself, so the text holds no term placeholder (validate-term-sinks)."
    }
    CustomStringTableItem "{3A87AA0CD67A55F9}" {
     Id "LF-Item_HandsetRadio_Name"
     Target_en_us "Radio set"
     Comment "Item name: a civilian handheld radio set that calls services."
    }
    CustomStringTableItem "{8927D8A09220929D}" {
     Id "LF-Item_HoneyJar"
     Target_en_us "Honey jar"
     Comment "Item name: a jar of honey made from two honeycombs at a kitchen (#3448)."
    }
    CustomStringTableItem "{9A31E991372C98FD}" {
     Id "LF-Item_HoneyJarDesc"
     Target_en_us "A jar of local honey. Sells slowly but steadily."
     Comment "Item description: honey jar (#3448)."
    }
    CustomStringTableItem "{3F0A5624D409C535}" {
     Id "LF-Item_Honeycomb"
     Target_en_us "Honeycomb"
     Comment "Item name: a frame of honeycomb taken from a beehive (#3447)."
    }
    CustomStringTableItem "{29701E64E31CE659}" {
     Id "LF-Item_HoneycombDesc"
     Target_en_us "A frame of honeycomb, heavy with honey."
     Comment "Item description: honeycomb (#3447)."
    }
    CustomStringTableItem "{D905B60D78F7B540}" {
     Id "LF-Item_Keycard_Description"
     Target_en_us "A cloned access card. The serial is printed on the back."
     Comment "Item description of the fence keycard."
    }
    CustomStringTableItem "{4CC4FC1BDF31889F}" {
     Id "LF-Item_Keycard_Name"
     Target_en_us "Keycard"
     Comment "Item name of the fence keycard."
    }
    CustomStringTableItem "{65C716E04B111614}" {
     Id "LF-Item_Limestone_Description"
     Target_en_us "A heavy block of limestone. A works turns it into cement."
     Comment "Item description of the limestone block (#3230)."
    }
    CustomStringTableItem "{9FF73E1CEF233CFB}" {
     Id "LF-Item_Limestone_Name"
     Target_en_us "Limestone"
     Comment "Item name of the limestone block."
    }
    CustomStringTableItem "{80906A73087767D7}" {
     Id "LF-Item_Lockpick_Description"
     Target_en_us "A set of picks. Each opened lock or snapped pick wears one use."
     Comment "Item description (#411)."
    }
    CustomStringTableItem "{2B581C5AF2C6DBC6}" {
     Id "LF-Item_Lockpick_Name"
     Target_en_us "Lockpick"
     Comment "Item name (#411)."
    }
    CustomStringTableItem "{B7CF3EC52549209E}" {
     Id "LF-Item_MarkedCash"
     Target_en_us "Marked notes"
     Comment "Item name: stolen banknotes still in a bank band; they look like cash but no shop or ATM takes them"
    }
    CustomStringTableItem "{4D5896C4AF350BBF}" {
     Id "LF-Item_MarkedCash_Desc"
     Target_en_us "Banknotes still in the bank's band. No shop or teller will take them as they are."
     Comment "Item description of the marked notes"
    }
    CustomStringTableItem "{315A7D38264FAED4}" {
     Id "LF-Item_MoneyBag"
     Target_en_us "Cash sack"
     Comment "Bank vault money bag item name"
    }
    CustomStringTableItem "{44F201EB4501CD73}" {
     Id "LF-Item_MoneyBag_Desc"
     Target_en_us "A heavy sack of banknotes from the vault, carried by the neck."
     Comment "Bank money bag item description"
    }
    CustomStringTableItem "{798CD609229F3E62}" {
     Id "LF-Item_Pager_Description"
     Target_en_us "Beeps when a call comes in for your service while you are on duty. Look at it to read the last calls."
     Comment "Inventory description of the pager."
    }
    CustomStringTableItem "{B5DC3AD9F79C05C4}" {
     Id "LF-Item_Pager_Name"
     Target_en_us "Pager"
     Comment "Inventory name of the pager."
    }
    CustomStringTableItem "{867133165FC03CB2}" {
     Id "LF-Item_Parcel"
     Target_en_us "Parcel"
     Comment "Courier parcel item name"
    }
    CustomStringTableItem "{4A1C9E27B05D3F18}" {
     Id "LF-Item_Parcel_Deliver"
     Target_en_us "Deliver to: %1"
     Comment "Parcel description line. %1 is the destination name (LF-Job_ParcelDest)."
    }
    CustomStringTableItem "{836BCE9515B5DC47}" {
     Id "LF-Item_Parcel_Desc"
     Target_en_us "A taped cardboard parcel for the drop-off address."
     Comment "Courier parcel item description"
    }
    CustomStringTableItem "{A342651F913CD658}" {
     Id "LF-Item_RadarGun_Description"
     Target_en_us "Hand-held speed radar. Aim at a car and use Measure speed. On-duty officers only."
     Comment "Inventory description of the police radar gun."
    }
    CustomStringTableItem "{3A0DC1946B0A4795}" {
     Id "LF-Item_RadarGun_Name"
     Target_en_us "Radar gun"
     Comment "Inventory name of the police radar gun."
    }
    CustomStringTableItem "{9195B0483F2D0148}" {
     Id "LF-Item_RepairKit_Description"
     Target_en_us "A toolbox of wrenches and patches. Holds one repair on a damaged vehicle."
     Comment "Item description (#3332)."
    }
    CustomStringTableItem "{1B57B817BF5340F0}" {
     Id "LF-Item_RepairKit_Name"
     Target_en_us "Repair kit"
     Comment "Item name (#3332)."
    }
    CustomStringTableItem "{C387085823DBA47D}" {
     Id "LF-Item_Scanner_Description"
     Target_en_us "Listens in on police and medic calls. Receives only. Illegal: an officer who finds it takes it and files a charge."
     Comment "Item description of the police scanner."
    }
    CustomStringTableItem "{824451A6C3821C83}" {
     Id "LF-Item_Scanner_Name"
     Target_en_us "Police scanner"
     Comment "Item name of the fence police scanner (#253)."
    }
    CustomStringTableItem "{8E63383FBBBFBE83}" {
     Id "LF-JailBail_Action"
     Target_en_us "Pay bail"
     Comment "Action on the jail bail phone."
    }
    CustomStringTableItem "{0D1924F8D487CAE9}" {
     Id "LF-JailBail_Desc"
     Target_en_us "Pay your bail from the bank and walk out"
     Comment "Action description on the jail bail phone."
    }
    CustomStringTableItem "{5E7B829175406CAC}" {
     Id "LF-JailBail_NoAccount"
     Target_en_us "There is no bank account to pay bail from."
     Comment "Toast: the prisoner has no bank account."
    }
    CustomStringTableItem "{83D41EBE07474BB4}" {
     Id "LF-JailBail_NotServing"
     Target_en_us "You are not serving a sentence."
     Comment "Toast: someone not in custody used the bail phone."
    }
    CustomStringTableItem "{C9FBAB97BE1A14A2}" {
     Id "LF-JailBail_Off"
     Target_en_us "Bail is not offered here."
     Comment "Toast and offer reason: the owner switched bail off."
    }
    CustomStringTableItem "{4C2DDD8EF971F0BA}" {
     Id "LF-JailBail_Paid"
     Target_en_us "Bail of %1 paid from your bank. You are free to go."
     Comment "Toast: bail paid. %1 = the bail."
    }
    CustomStringTableItem "{65A1EF059B6C37A4}" {
     Id "LF-JailBail_TooPoor"
     Target_en_us "Bail is %1. Your bank cannot cover it."
     Comment "Toast: the bank holds less than the bail. %1 = the bail."
    }
    CustomStringTableItem "{B0C471156E9A5AB6}" {
     Id "LF-JailBook_AtStation"
     Target_en_us "At the jail. Walk the suspect to the cell and book them from the action list."
     Comment "Toast to an officer who took a suspect out of the car at the jail."
    }
    CustomStringTableItem "{7B1CB5C7C37EE4F4}" {
     Id "LF-JailBook_Booked"
     Target_en_us "Booked. The prisoner is in the cell and can see their time."
     Comment "Toast to the officer after a booking."
    }
    CustomStringTableItem "{4EA07E3FC70C2632}" {
     Id "LF-JailBook_CuffFirst"
     Target_en_us "Cuff the suspect first: hands up and unarmed, or downed."
     Comment "Toast or reason: escort and booking need the suspect cuffed."
    }
    CustomStringTableItem "{D3DE7C61E0707EFF}" {
     Id "LF-JailBook_Failed"
     Target_en_us "The booking did not go through. The suspect may not be wanted any more."
     Comment "Toast to the officer when the jail refused the booking."
    }
    CustomStringTableItem "{5EEAFA2D61673F01}" {
     Id "LF-JailBook_FirstArrest"
     Target_en_us "Bring the suspect to the jail booking point (waypoint set). Escort them, or put them in your car, then book them there."
     Comment "First-arrest note to an officer, once per session."
    }
    CustomStringTableItem "{570E5C4A7E0D9CF9}" {
     Id "LF-JailBook_Full"
     Target_en_us "Book into jail: full sentence"
     Comment "Officer action on a cuffed suspect at the jail cell."
    }
    CustomStringTableItem "{A8913ACFC05D09B8}" {
     Id "LF-JailBook_NotHere"
     Target_en_us "Book the suspect at the jail. Follow the waypoint."
     Comment "Toast to an officer who tried to book away from the cell."
    }
    CustomStringTableItem "{93DF63C0D0C1819E}" {
     Id "LF-JailBook_Reduced"
     Target_en_us "Book into jail: reduced (cooperated)"
     Comment "Officer action on a cuffed suspect at the jail cell: half the sentence."
    }
    CustomStringTableItem "{C3E13CE15439D1F5}" {
     Id "LF-JailBook_Waypoint"
     Target_en_us "Suspect cuffed. Waypoint set to the jail."
     Comment "Toast to the officer after a cuff of a wanted suspect."
    }
    CustomStringTableItem "{D75A5C9E7EA2F993}" {
     Id "LF-JailBook_WaypointLabel"
     Target_en_us "Jail booking"
     Comment "Waypoint label for the jail booking point."
    }
    CustomStringTableItem "{7DAABAD2EC8E0E3D}" {
     Id "LF-JailBreak_Cooldown"
     Target_en_us "The jail was broken open not long ago. Wait before you try again."
     Comment "Toast: the jail doors are inside the cooldown after a break."
    }
    CustomStringTableItem "{0AE487F4A2029D85}" {
     Id "LF-JailBreak_FromInside"
     Target_en_us "You can't set a charge from inside the cell."
     Comment "Toast: a serving prisoner tried to charge a jail door."
    }
    CustomStringTableItem "{A5E0EB1EBEB9C3DF}" {
     Id "LF-JailBreak_OuterFirst"
     Target_en_us "Blow the outer door first."
     Comment "Toast and offer reason: the cell door waits until the jail's outer door is open."
    }
    CustomStringTableItem "{E723DD95A9CA332C}" {
     Id "LF-JailClock_Line"
     Target_en_us "Jail: %1 left"
     Comment "Prisoner HUD line; %1 is m:ss."
    }
    CustomStringTableItem "{B248F0FEF12E26A8}" {
     Id "LF-JailSlip_Blank"
     Target_en_us "A blank sentence slip."
     Comment "Sentence slip description before it is printed."
    }
    CustomStringTableItem "{AB8EEC396FC517C0}" {
     Id "LF-JailSlip_MinutesLeft"
     Target_en_us "Time left: %1 min, served while you are online"
     Comment "Sentence slip line. %1 is the whole online minutes left."
    }
    CustomStringTableItem "{26F8FC7CB843012A}" {
     Id "LF-JailSlip_Name"
     Target_en_us "Sentence slip"
     Comment "Inventory name of the paper a prisoner receives on sentencing."
    }
    CustomStringTableItem "{FD775A4BC17E874E}" {
     Id "LF-JailSlip_Offence"
     Target_en_us "Offense: wanted level %1"
     Comment "Sentence slip line. %1 is the wanted level at sentencing."
    }
    CustomStringTableItem "{37A65DC6C4FED493}" {
     Id "LF-JailSlip_Prisoner"
     Target_en_us "Prisoner: %1"
     Comment "Sentence slip line. %1 is the prisoner name."
    }
    CustomStringTableItem "{67C253FDB532D573}" {
     Id "LF-JailSlip_Stamped"
     Target_en_us "SERVED. Released."
     Comment "Stamp printed on a sentence slip when the sentence ends."
    }
    CustomStringTableItem "{5D1E7A3C9B20F486}" {
     Id "LF-JailSlip_Unnamed"
     Target_en_us "unknown"
     Comment "Sentence slip: the prisoner name when the prisoner has no character name. Never the platform uid."
    }
    CustomStringTableItem "{04A2CD75730658F5}" {
     Id "LF-JailWork_BadConfig"
     Target_en_us "This job is not set up."
     Comment "Jail job refused: no seconds configured."
    }
    CustomStringTableItem "{9D3D515C89CBD67C}" {
     Id "LF-JailWork_Busy"
     Target_en_us "You are already working."
     Comment "Jail job refused: a job is already running."
    }
    CustomStringTableItem "{C1C1BA78E72558F0}" {
     Id "LF-JailWork_Cooldown"
     Target_en_us "You need a rest. This job is free again in %1."
     Comment "Jail job refused: cooldown. %1 is m:ss."
    }
    CustomStringTableItem "{F9D02BE97AECAA25}" {
     Id "LF-JailWork_Done"
     Target_en_us "Job done. Time left: %1"
     Comment "Toast after a jail job. %1 is the remaining sentence as m:ss."
    }
    CustomStringTableItem "{0B188B4AD2B2F068}" {
     Id "LF-JailWork_Kitchen"
     Target_en_us "Wash the dishes"
     Comment "Action at the jail kitchen sink: wash dishes to shorten the sentence."
    }
    CustomStringTableItem "{B2F316158144F2CB}" {
     Id "LF-JailWork_Laundry"
     Target_en_us "Sort the laundry"
     Comment "Action in the cell: sort laundry to shorten the sentence."
    }
    CustomStringTableItem "{9149AEEB14C38188}" {
     Id "LF-JailWork_LaundryTub"
     Target_en_us "Scrub the laundry"
     Comment "Action at the jail laundry tub: scrub laundry to shorten the sentence."
    }
    CustomStringTableItem "{7AB47127A2E6C90C}" {
     Id "LF-JailWork_NotPrisoner"
     Target_en_us "Only a prisoner serving a sentence can work here."
     Comment "Jail job refused: the player is not serving."
    }
    CustomStringTableItem "{950892F5D530ACC9}" {
     Id "LF-JailWork_Scrub"
     Target_en_us "Scrub the floor"
     Comment "Action in the cell: scrub the floor to shorten the sentence."
    }
    CustomStringTableItem "{D5FF62032D49E07F}" {
     Id "LF-JailWork_Yard"
     Target_en_us "Sweep the yard"
     Comment "Action at the jail yard broom: sweep the yard to shorten the sentence."
    }
    CustomStringTableItem "{6C2C86A5611DFD9A}" {
     Id "LF-Jail_SlipHanded"
     Target_en_us "You were handed a sentence slip. Check your inventory."
     Comment "Toast when a prisoner receives the sentence slip."
    }
    CustomStringTableItem "{3E7A91C0D5B24F68}" {
     Id "LF-JobMap_Fare"
     Target_en_us "Fare"
     Comment "Map mark and waypoint label: the destination of an accepted taxi fare (driver and passenger)."
    }
    CustomStringTableItem "{6B2C5C7E28A957E6}" {
     Id "LF-JobMap_PostLoad"
     Target_en_us "Post office"
     Comment "Map mark and waypoint label: the office a postal run starts at (the pack names it)."
    }
    CustomStringTableItem "{74032FC49746CAB7}" {
     Id "LF-JobMap_PostReturn"
     Target_en_us "Post office"
     Comment "Map mark and waypoint label: the office a postal run returns the van to (the pack names it)."
    }
    CustomStringTableItem "{78DC7678389C4C63}" {
     Id "LF-JobMap_PostUnload"
     Target_en_us "Post office"
     Comment "Map mark and waypoint label: the office a postal run delivers to (the pack names it)."
    }
    CustomStringTableItem "{8C41F6B2A09D3E75}" {
     Id "LF-JobMap_Stop"
     Target_en_us "Delivery stop"
     Comment "Map mark and waypoint label: the current stop of a delivery run (courier, fuel)."
    }
    CustomStringTableItem "{ADAD471556F7297F}" {
     Id "LF-Job_AbandonedDeposit"
     Target_en_us "Run abandoned. No pay, and the {currency:sym}%1 deposit is kept."
     Comment "Notice when a delivery run with a cargo deposit is abandoned; %1 is the kept deposit."
    }
    CustomStringTableItem "{CDE67184D007F114}" {
     Id "LF-Job_AccessDenied"
     Target_en_us "Access denied"
     Target_es_es "Acceso denegado"
     Comment "Hint title when a job cannot be taken"
    }
    CustomStringTableItem "{A3F1B7C2D94E6A01}" {
     Id "LF-Job_AlreadyRun"
     Target_en_us "You already have a delivery run. Abandon it at the counter first."
     Comment "Toast: Pick up refused because the player still has an open run (#3356); the title is the job name."
    }
    CustomStringTableItem "{0DAF48DFDAAF2069}" {
     Id "LF-Job_Desc_Medic"
     Target_en_us "Emergency ambulance crew. Treats the injured on the public payroll."
     Target_es_es "Paramédico de emergencias. Atiende a los heridos con sueldo público."
     Comment "Description of the ambulance service job"
    }
    CustomStringTableItem "{4CA1C4431CA9A325}" {
     Id "LF-Job_Desc_Police"
     Target_en_us "Sworn police officer. Requires an approved appointment."
     Target_es_es "Oficial de policía. Requiere un nombramiento aprobado."
     Comment "Description of the police job (appointment = whitelist)"
    }
    CustomStringTableItem "{4AD1BED424C85624}" {
     Id "LF-Job_DeskQuit"
     Target_en_us "Quit the job: %1"
     Comment "Workplace desk action when the player already holds the job. %1 is the job name."
    }
    CustomStringTableItem "{0253B3296037A056}" {
     Id "LF-Job_DeskTake"
     Target_en_us "Take the job: %1"
     Comment "Workplace desk action. %1 is the job name (Miner, Lumberjack, Farmer)."
    }
    CustomStringTableItem "{CD5B6F2CE37D13FD}" {
     Id "LF-Job_DeskTakeName"
     Target_en_us "Take the job"
     Comment "Workplace desk action name in the prefab (the script prints the job name)."
    }
    CustomStringTableItem "{2FFBFD36EA8DB8AE}" {
     Id "LF-Job_FruitHarvest"
     Target_en_us "You picked %1 %2."
     Target_es_es "Recolectaste %1 %2."
     Comment "Fruit picking result. %1 = count, %2 = fruit name (plural, lowercase)"
    }
    CustomStringTableItem "{6E2A6680A9D480CA}" {
     Id "LF-Job_GatherApples_Desc"
     Target_en_us "Gather 5 apples"
     Comment "Legacy job board apple gather action description"
    }
    CustomStringTableItem "{E064585F9B6FED1B}" {
     Id "LF-Job_GatherApples_Name"
     Target_en_us "Gather apples"
     Comment "Legacy job board apple gather action name"
    }
    CustomStringTableItem "{5374FA2BFDB1A7AE}" {
     Id "LF-Job_Hired"
     Target_en_us "You now work as %1, grade %2."
     Target_es_es "Ahora trabajas como %1, categoría %2."
     Comment "Hint text when the player takes a civilian job. %1 = job title, %2 = grade (job level)"
    }
    CustomStringTableItem "{196A4BD9FFC674C5}" {
     Id "LF-Job_HiredHidden"
     Target_en_us "You now work as %1."
     Comment "Hint when hired; %1 = job name. Hidden progression: no grade or level."
    }
    CustomStringTableItem "{60E43C3EAEE492E1}" {
     Id "LF-Job_Hired_Title"
     Target_en_us "New employment"
     Target_es_es "Nuevo empleo"
     Comment "Hint title when the player takes a civilian job"
    }
    CustomStringTableItem "{50FCD7F3D7EB972C}" {
     Id "LF-Job_MoreRunsFirst"
     Target_en_us "Do more parcel runs first"
     Comment "Greyed job sign-up reason: the player needs more delivery runs. Hidden progression: no level is named."
    }
    CustomStringTableItem "{695CFE5E8782D705}" {
     Id "LF-Job_Name_Farmer"
     Target_en_us "Farm hand"
     Target_es_es "Peón agrícola"
     Comment "Job title: agricultural worker (1989 employment form wording)"
    }
    CustomStringTableItem "{EBFC5BE844D9E9E3}" {
     Id "LF-Job_Name_Lumberjack"
     Target_en_us "Logger"
     Target_es_es "Leñador"
     Comment "Job title: forestry worker"
    }
    CustomStringTableItem "{7199A0B3FCE020A3}" {
     Id "LF-Job_Name_Medic"
     Target_en_us "Ambulance crew"
     Target_es_es "Servicio de ambulancias"
     Comment "Job title: emergency medical (ambulance) service"
    }
    CustomStringTableItem "{81696576D9405DAC}" {
     Id "LF-Job_Name_Miner"
     Target_en_us "Miner"
     Target_es_es "Minero"
     Comment "Job title: miner"
    }
    CustomStringTableItem "{FDFA125F576C29FB}" {
     Id "LF-Job_Name_Police"
     Target_en_us "Police officer"
     Target_es_es "Policía"
     Comment "Job title: police officer"
    }
    CustomStringTableItem "{F4140C183366F34A}" {
     Id "LF-Job_Name_Unemployed"
     Target_en_us "Civilian"
     Target_es_es "Civil"
     Comment "Job title of a player with no employment"
    }
    CustomStringTableItem "{446722059437550F}" {
     Id "LF-Job_Name_Unknown"
     Target_en_us "Unknown post"
     Target_es_es "Puesto desconocido"
     Comment "Job title shown when the player's job config is missing"
    }
    CustomStringTableItem "{3129D349B0449B3A}" {
     Id "LF-Job_NeedDeposit"
     Target_en_us "You need {currency:sym}%1 in cash as a deposit for the cargo. You get it back when you deliver."
     Comment "Toast: a run does not start because the player cannot cover the cargo deposit; %1 is the deposit; the title is the job name."
    }
    CustomStringTableItem "{A6F0C19B47735D9E}" {
     Id "LF-Job_NeedMedicCard"
     Target_en_us "You need your ambulance service ID to go on duty. It is issued at the hospital."
     Target_es_es "Necesitas la credencial del servicio de ambulancias para entrar de servicio. Se expide en el hospital."
     Comment "Ambulance job refused: the player lacks the ambulance credential licence"
    }
    CustomStringTableItem "{0F3FC6994B2A56D8}" {
     Id "LF-Job_NeedPoliceBadge"
     Target_en_us "You need your {police} warrant card to go on duty. It is issued at the station."
     Target_es_es "Necesitas la placa policial para entrar de servicio. Se expide en la comisaría."
     Comment "Police job refused: the player lacks the police credential licence"
    }
    CustomStringTableItem "{F51D6FC3BD1F68DC}" {
     Id "LF-Job_NewWork"
     Target_en_us "The clerk has more work for you now. Sign up at the depot counter."
     Comment "Toast when job experience opens a new delivery job; the title is the job name. No level is shown."
    }
    CustomStringTableItem "{D604EAF50C719D34}" {
     Id "LF-Job_NoCargo"
     Target_en_us "You are not carrying the cargo. Pick it up again, or abandon the run at the counter."
     Comment "Toast: a drop-off refused because the cargo is not in the player's inventory (#3356); the title is the job name."
    }
    CustomStringTableItem "{12050BC151134136}" {
     Id "LF-Job_NoPassenger"
     Target_en_us "A fare needs a passenger seated in your car."
     Comment "Toast: a taxi fare is refused at the drop-off because no other player is seated in the driver vehicle; the title is the job name."
    }
    CustomStringTableItem "{C5F3D9E4FB608C23}" {
     Id "LF-Job_NoRoom"
     Target_en_us "You have no room to carry the cargo. Make space and ask again."
     Comment "Toast: a run start refused because the cargo did not fit in the inventory (#3356); the title is the job name."
    }
    CustomStringTableItem "{3B4E65464D2780EE}" {
     Id "LF-Job_NotADrop"
     Target_en_us "This is not a drop-off point."
     Comment "Toast: the entity the drop-off was tried on is not a placed drop site; the title is the job name."
    }
    CustomStringTableItem "{5F637FC76D6C6D7E}" {
     Id "LF-Job_NotWhitelisted"
     Target_en_us "You are not on the roster for this post. Speak to an administrator."
     Target_es_es "No figuras en la plantilla de este puesto. Habla con un administrador."
     Comment "Restricted job refused: the player is not whitelisted"
    }
    CustomStringTableItem "{C474DE8385A4FAC0}" {
     Id "LF-Job_OffDuty"
     Target_en_us "You have ended your shift."
     Target_es_es "Has finalizado tu turno de servicio."
     Comment "Hint text when the player ends a shift"
    }
    CustomStringTableItem "{26D3AE4AB6FDC686}" {
     Id "LF-Job_OffDuty_Title"
     Target_en_us "Off duty"
     Target_es_es "Fuera de servicio"
     Comment "Hint title when the player ends a shift"
    }
    CustomStringTableItem "{7A7A526CF01F6A86}" {
     Id "LF-Job_OnDutyMedic"
     Target_en_us "On duty as %1. Ambulance crew standing by."
     Target_es_es "De servicio como %1. Ambulancia operativa."
     Comment "Hint text when an ambulance crew member starts a shift. %1 = job title"
    }
    CustomStringTableItem "{71853BEF3A3822F3}" {
     Id "LF-Job_OnDutyMedic_Title"
     Target_en_us "Ambulance on duty"
     Target_es_es "Ambulancia de servicio"
     Comment "Hint title when an ambulance crew member starts a shift"
    }
    CustomStringTableItem "{B8F62B559B06F232}" {
     Id "LF-Job_OnDutyPolice"
     Target_en_us "On duty as %1. 10-4, unit on patrol."
     Target_es_es "De servicio como %1. 10-4, unidad en patrulla."
     Comment "Hint text when a police officer starts a shift. %1 = job title"
    }
    CustomStringTableItem "{52D2FDF4AA85DFF5}" {
     Id "LF-Job_OnDutyPolice_Title"
     Target_en_us "{Police} on duty"
     Target_es_es "{Police} de servicio"
     Comment "Hint title when a police officer starts a shift"
    }
    CustomStringTableItem "{3148B2D8F16CAE32}" {
     Id "LF-Job_Paid_Bonus"
     Target_en_us "Paid {currency:sym}%1: run {currency:sym}%2, bonus +{currency:sym}%3"
     Comment "Toast: a delivery run with a contract or standing bonus (#3148). %1 = all of it, %2 = the run's pay, %3 = the bonus."
    }
    CustomStringTableItem "{3148A1C7E05B9D21}" {
     Id "LF-Job_Paid_Night"
     Target_en_us "Paid {currency:sym}%1: run {currency:sym}%2, night +{currency:sym}%3"
     Comment "Toast: a delivery run finished at night (#3148). %1 = all of it, %2 = the run's pay, %3 = the night rate's extra."
    }
    CustomStringTableItem "{3148C3E90A27BF43}" {
     Id "LF-Job_Paid_NightBonus"
     Target_en_us "Paid {currency:sym}%1: run {currency:sym}%2, night +{currency:sym}%3, bonus +{currency:sym}%4"
     Comment "Toast: a delivery run finished at night with a bonus (#3148). %1 = all of it, %2 = the run's pay, %3 = the night extra, %4 = the bonus."
    }
    CustomStringTableItem "{4A1C9E27B05D3F19}" {
     Id "LF-Job_ParcelDest"
     Target_en_us "the delivery stop marked on your map"
     Comment "Where a courier parcel goes. Shown in the parcel description."
    }
    CustomStringTableItem "{967C561794E4E4EC}" {
     Id "LF-Job_Paycheck"
     Target_en_us "Wages paid into your bank account: %1"
     Target_es_es "Salario ingresado en tu cuenta bancaria: %1"
     Comment "Hint text on payday. %1 = formatted amount. Title is the job title"
    }
    CustomStringTableItem "{CF29236D0C626F05}" {
     Id "LF-Job_Promoted"
     Target_en_us "%1, grade %2. Practice pays: you bring in %3% more per shift."
     Target_es_es "%1, categoría %2. La práctica se nota: rindes un %3% más por turno."
     Comment "Hint text when job grade rises. %1 = job title, %2 = grade, %3 = yield bonus percent"
    }
    CustomStringTableItem "{AF990714687E46C1}" {
     Id "LF-Job_Promoted_Title"
     Target_en_us "Promotion"
     Target_es_es "Ascenso"
     Comment "Hint title when the player's job grade rises (employment fiction, not an arcade level-up)"
    }
    CustomStringTableItem "{B72D78719E88C276}" {
     Id "LF-Job_RunStarted_CourierDeposit"
     Target_en_us "Carry the parcel to the delivery stop marked on your map, then Complete the run. Your {currency:sym}%1 deposit comes back on delivery."
     Comment "Toast when a courier picks up a parcel: where the drop is and the cash deposit taken at the counter (%1). Capitalized phrase is the drop-off action name."
    }
    CustomStringTableItem "{958D10C1086B7C3E}" {
     Id "LF-Job_SignedUp"
     Target_en_us "You are signed up. Start a run at the counter when you are ready."
     Comment "Toast after a delivery-job sign-up at the post office counter."
    }
    CustomStringTableItem "{618A5B0E5F6A7C85}" {
     Id "LF-Job_SignedUp_Courier"
     Target_en_us "You are a courier now. Pick up a parcel at this counter."
     Comment "Toast after signing up for courier work: the next step."
    }
    CustomStringTableItem "{CF7F0BA17E3A08CD}" {
     Id "LF-Job_SignedUp_Fuel"
     Target_en_us "You deliver fuel now. Take a fuel can at this counter."
     Comment "Toast after signing up for fuel delivery: the next step."
    }
    CustomStringTableItem "{368CBF7BF68CC9AE}" {
     Id "LF-Job_SignedUp_Taxi"
     Target_en_us "You drive a taxi now. Start a fare at this counter."
     Comment "Toast after signing up for taxi work: the next step."
    }
    CustomStringTableItem "{B4E2C8D3EA5F7B12}" {
     Id "LF-Job_StartRefused"
     Target_en_us "The counter cannot start that run right now."
     Comment "Toast: a run start refused for a reason with no better words (#3356); the title is the job name."
    }
    CustomStringTableItem "{4BBC06DC40BA877A}" {
     Id "LF-Job_TooClose"
     Target_en_us "That drop is too close to where you started. Take it farther away."
     Comment "Toast: a run is refused at a drop nearer than the configured minimum distance from its start; the title is the job name."
    }
    CustomStringTableItem "{F83B7CF7B24C02B9}" {
     Id "LF-Job_TooFast"
     Target_en_us "You got here too quickly to have made the trip. Nothing is paid."
     Comment "Toast: a run is refused because it took less than the configured share of the expected driving time; the title is the job name."
    }
    CustomStringTableItem "{3B3C747588B68991}" {
     Id "LF-Job_Work_Title"
     Target_en_us "Work"
     Target_es_es "Trabajo"
     Comment "Hint title for work results"
    }
    CustomStringTableItem "{35AF0FE5D6C58F4F}" {
     Id "LF-Keybind_MapHelpToggle"
     Target_en_us "Map: show or hide the help card"
     Comment "Key binding menu entry for folding the map help card."
    }
    CustomStringTableItem "{59804671C301B6FE}" {
     Id "LF-Keybind_MapWaypointClear"
     Target_en_us "Map: clear waypoint"
     Comment "Keybinding menu: clears the personal waypoint while the map is open (Backspace, gamepad Y twice)."
    }
    CustomStringTableItem "{76177DC570F67928}" {
     Id "LF-Keybind_MapWaypointModifier"
     Target_en_us "Map: set waypoint (hold, then left click)"
     Comment "Keybinding menu: the modifier held with a map click to set the waypoint (keyboard Shift, gamepad Y)."
    }
    CustomStringTableItem "{280FC8E5DDE60C95}" {
     Id "LF-Keybind_OpenEmote"
     Target_en_us "Open emote menu"
     Comment "Key binding menu label: hold to open the emote radial."
    }
    CustomStringTableItem "{15BA4CF2BECFEB7F}" {
     Id "LF-Keybind_SirenDouble"
     Target_en_us "Cycle siren mode (alternate)"
     Comment "Key binding menu label: cycle the vehicle siren modes (double click)."
    }
    CustomStringTableItem "{EBE5A15AF131A672}" {
     Id "LF-Keybind_SirenSingle"
     Target_en_us "Cycle siren mode"
     Comment "Key binding menu label: cycle the vehicle siren modes (single click)."
    }
    CustomStringTableItem "{C8B8911B5F3A014D}" {
     Id "LF-Keybind_VehicleLock"
     Target_en_us "Lock or unlock vehicle"
     Comment "Key binding menu label for the U key."
    }
    CustomStringTableItem "{A81D7E3C7F1A56FE}" {
     Id "LF-Launder_Busy"
     Target_en_us "Someone else has a batch in."
     Comment "Toast: the laundering station already holds a batch (#2853)."
    }
    CustomStringTableItem "{CD8D5A8DDA7422F4}" {
     Id "LF-Launder_Closed"
     Target_en_us "Nobody is washing money today."
     Comment "Toast: the laundering rules config is missing or refused (#2853)."
    }
    CustomStringTableItem "{9D01404950D59291}" {
     Id "LF-Launder_Collect"
     Target_en_us "Take the clean cash"
     Comment "Laundering station action: take the washed batch out as cash (#2853)."
    }
    CustomStringTableItem "{AFB662F9E2F3676A}" {
     Id "LF-Launder_Collected"
     Target_en_us "You take the clean cash."
     Comment "Toast: clean cash taken from the laundering station (#2853)."
    }
    CustomStringTableItem "{DC272F956DFB6D44}" {
     Id "LF-Launder_DailyCap"
     Target_en_us "You have washed all you can today. Come back tomorrow."
     Comment "Toast: the character reached the 24 h laundering cap (#2853)."
    }
    CustomStringTableItem "{424DC80066E431A2}" {
     Id "LF-Launder_Empty"
     Target_en_us "There is nothing in the station."
     Comment "Toast: collect at an empty laundering station (#2853)."
    }
    CustomStringTableItem "{B9557E043B1D76EF}" {
     Id "LF-Launder_Load"
     Target_en_us "Hand over marked notes"
     Comment "Laundering station action: put marked notes in to wash (#2853)."
    }
    CustomStringTableItem "{0F28F6C90EE350AF}" {
     Id "LF-Launder_Loaded"
     Target_en_us "The notes are in. Come back when they are clean."
     Comment "Toast: marked notes loaded into the laundering station (#2853)."
    }
    CustomStringTableItem "{0319442FDC722D7C}" {
     Id "LF-Launder_NoNotes"
     Target_en_us "You have no marked notes to wash."
     Comment "Toast and cannot-perform reason: no marked notes on the character (#2853)."
    }
    CustomStringTableItem "{CD22B4916812EB71}" {
     Id "LF-Launder_NotReady"
     Target_en_us "The notes are not clean yet."
     Comment "Toast: collect before the batch time has passed (#2853)."
    }
    CustomStringTableItem "{9230020C15FC4430}" {
     Id "LF-Launder_NotYours"
     Target_en_us "That batch is not yours."
     Comment "Toast: another character tried to take someone else's batch (#2853)."
    }
    CustomStringTableItem "{CE4833F0D2918E75}" {
     Id "LF-Launder_Title"
     Target_en_us "Laundering"
     Comment "Toast title at the laundering station (#2853)."
    }
    CustomStringTableItem "{CA24071CBF88363F}" {
     Id "LF-Launder_Washing"
     Target_en_us "Check the wash"
     Comment "Laundering station action while the batch is still washing (#2853)."
    }
    CustomStringTableItem "{27381134AAEF8E1F}" {
     Id "LF-LawBook_Empty"
     Target_en_us "No codes in this section."
     Comment "Law-book page line when a section lists no codes."
    }
    CustomStringTableItem "{9DDDB9A7239FF396}" {
     Id "LF-LawBook_LineFine"
     Target_en_us "%1  {currency:sym}%2"
     Comment "Law-book page line for a code with a fine and no time: %1 code name, %2 fine in dollars."
    }
    CustomStringTableItem "{81090FA397167E8F}" {
     Id "LF-LawBook_LineFineTime"
     Target_en_us "%1  {currency:sym}%2, %3 min"
     Comment "Law-book page line: %1 code name, %2 fine in dollars, %3 time in minutes."
    }
    CustomStringTableItem "{F8582E4A8E0A84F1}" {
     Id "LF-LawBook_LineJail"
     Target_en_us "%1  jail only, %2 min"
     Comment "Law-book page line for a jail-only code (no fine): %1 code name, %2 time in minutes."
    }
    CustomStringTableItem "{19E1E2EEA95E7990}" {
     Id "LF-LawBook_Read"
     Target_en_us "Read: %1"
     Comment "Action on the law-book lectern, one per section. %1 = the section name."
    }
    CustomStringTableItem "{C9966D6AE8C46169}" {
     Id "LF-LawBook_Section_criminal"
     Target_en_us "Criminal"
     Comment "Law-book section name: criminal codes."
    }
    CustomStringTableItem "{79D1B91FDF7A5E37}" {
     Id "LF-LawBook_Section_narcotics"
     Target_en_us "Narcotics"
     Comment "Law-book section name: narcotics codes."
    }
    CustomStringTableItem "{A0D0B472BFD72AF7}" {
     Id "LF-LawBook_Section_road"
     Target_en_us "Traffic"
     Comment "Law-book section name: road and traffic codes."
    }
    CustomStringTableItem "{36C97D557BFD5D82}" {
     Id "LF-LawBook_Section_weapons"
     Target_en_us "Weapons"
     Comment "Law-book section name: weapons codes."
    }
    CustomStringTableItem "{519671401E89B54B}" {
     Id "LF-LawBook_Title"
     Target_en_us "Law book: %1"
     Comment "Title of a law-book page. %1 = the section name."
    }
    CustomStringTableItem "{59EE09E1B7435797}" {
     Id "LF-LawlessHud_Enter"
     Target_en_us "You entered a lawless area. Server rules are relaxed here. Watch your back."
     Comment "Toast on entering a lawless zone (also when joining or spawning inside one)."
    }
    CustomStringTableItem "{9A2F1AA936E1BDA7}" {
     Id "LF-LawlessHud_Exit"
     Target_en_us "You left the lawless area."
     Comment "Toast on leaving a lawless zone."
    }
    CustomStringTableItem "{F297FFD8D4B6E791}" {
     Id "LF-LawlessHud_Tag"
     Target_en_us "LAWLESS AREA"
     Comment "Persistent HUD tag while the local player stands inside a lawless zone."
    }
    CustomStringTableItem "{4FBA54E7032A04DE}" {
     Id "LF-LawlessHud_Title"
     Target_en_us "Lawless area"
     Comment "Toast title when the player enters or leaves a lawless zone."
    }
    CustomStringTableItem "{3A7C91E5D2B40F68}" {
     Id "LF-LevelUpCard_Opens"
     Target_en_us "Opens: %1"
     Comment "Level-up HUD card (#4404). %1 = what the new level opens, the trade unlock text."
    }
    CustomStringTableItem "{AFDD3C3A428C4C18}" {
     Id "LF-Level_Bonus"
     Target_en_us "Yield +%1% | Sale price +%2%"
     Target_es_es "Rendimiento +%1% | Precio de venta +%2%"
     Comment "Character standing bonuses. %1 = gathering bonus percent, %2 = sale bonus percent"
    }
    CustomStringTableItem "{0B6BBD519DC9B906}" {
     Id "LF-Level_BonusNone"
     Target_en_us "No standing bonuses yet"
     Target_es_es "Sin bonos activos"
     Comment "Character standing summary when there are no bonuses"
    }
    CustomStringTableItem "{3AF129B888C991B7}" {
     Id "LF-Level_Far_Delivery"
     Target_en_us "Do more runs first."
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{99637675CD655B38}" {
     Id "LF-Level_Far_Farmer"
     Target_en_us "You have not worked the land enough for this yet."
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{1A467AE1429D59EA}" {
     Id "LF-Level_Far_Lumberjack"
     Target_en_us "You have not spent enough time in the forest for this yet."
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{F468C3116863E4C2}" {
     Id "LF-Level_Far_Medic"
     Target_en_us "You are not experienced enough in medicine for this yet."
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{B9DCBE4AEED66191}" {
     Id "LF-Level_Far_Miner"
     Target_en_us "You have not spent enough time mining for this yet."
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{41FE6820DDF1221C}" {
     Id "LF-Level_Far_Police"
     Target_en_us "You are not experienced enough in the force for this yet."
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{83D47B50E56450B1}" {
     Id "LF-Level_Near_Delivery"
     Target_en_us "Nearly there. A few more runs first."
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{96C6CD7CEE27AED3}" {
     Id "LF-Level_Near_Farmer"
     Target_en_us "Nearly there. A little more work in the fields first."
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{0001301D204BDAFA}" {
     Id "LF-Level_Near_Lumberjack"
     Target_en_us "Nearly there. A few more days felling timber first."
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{CE9DF1FD414FC63E}" {
     Id "LF-Level_Near_Medic"
     Target_en_us "Nearly there. A little more practice with patients first."
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{601CF8826758D265}" {
     Id "LF-Level_Near_Miner"
     Target_en_us "Nearly there. A few more shifts digging first."
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{01E474CC353E1714}" {
     Id "LF-Level_Near_Police"
     Target_en_us "Nearly there. A little more time on duty first."
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{50B4954BCAFEE2D0}" {
     Id "LF-Level_Needs"
     Target_en_us "Needs %1 %2"
     Comment "Level gate refusal for a trade with no words of its own (#4856). %1 = the trade, e.g. Smelting; %2 = the level, e.g. 8."
    }
    CustomStringTableItem "{D73232FD8F9ACA91}" {
     Id "LF-Level_NeedsAny"
     Target_en_us "Needs level %1"
     Comment "Level gate refusal when the gate names no trade (#4856). %1 = the level."
    }
    CustomStringTableItem "{2024E806595907BF}" {
     Id "LF-Level_Promoted"
     Target_en_us "Your record now reads grade %1. Your supervisor has put you down for further training."
     Target_es_es "Tu expediente ya figura en la categoría %1. Tu supervisor te ha propuesto para más formación."
     Comment "Hint text when overall standing rises. %1 = grade. Further training = one skill point to spend on a licence"
    }
    CustomStringTableItem "{8D0BA6972F630893}" {
     Id "LF-Level_PromotedBonus"
     Target_en_us "Experience shows: yield +%1%, sale price +%2%."
     Target_es_es "Se nota la experiencia: rendimiento +%1%, precio de venta +%2%."
     Comment "Appended to the standing hint. %1 = gathering bonus percent, %2 = sale bonus percent"
    }
    CustomStringTableItem "{53E56B72A7F27F7F}" {
     Id "LF-Level_Promoted_Title"
     Target_en_us "Standing improved"
     Target_es_es "Mejora de categoría"
     Comment "Hint title when the character's overall standing rises (replaces an arcade LEVEL UP)"
    }
    CustomStringTableItem "{9D8FF04C43EF328F}" {
     Id "LF-Level_TradeTier_1"
     Target_en_us "Apprentice"
     Comment "Trade tier word for levels 1 to 5, shown beside the trade on the F2 skill card (#4414). The pack may override it."
    }
    CustomStringTableItem "{F4ECAB69CD47C063}" {
     Id "LF-Level_TradeTier_2"
     Target_en_us "Hand"
     Comment "Trade tier word for levels 6 to 11 (#4414)."
    }
    CustomStringTableItem "{5F256D6141465559}" {
     Id "LF-Level_TradeTier_3"
     Target_en_us "Journeyman"
     Comment "Trade tier word for levels 12 to 19 (#4414)."
    }
    CustomStringTableItem "{50ED2B8D81EFB19E}" {
     Id "LF-Level_TradeTier_4"
     Target_en_us "Foreman"
     Comment "Trade tier word for levels 20 to 27 (#4414)."
    }
    CustomStringTableItem "{D9282509B6BB1C35}" {
     Id "LF-Level_TradeTier_5"
     Target_en_us "Master"
     Comment "Trade tier word for levels 28 to 30 (#4414)."
    }
    CustomStringTableItem "{FEB731F46AC70C21}" {
     Id "LF-LicenceBarred_Driving"
     Target_en_us "You cannot get a driver's license for %1 days."
     Comment "Toast to the arrested player who held no driver's license: the conviction bars them from one. %1 = in-game days."
    }
    CustomStringTableItem "{ACF2B58E9F84E064}" {
     Id "LF-LicenceBarred_Firearm"
     Target_en_us "You cannot get a firearm permit for %1 days."
     Comment "Toast to the arrested player who held no firearm permit: the conviction bars them from one. %1 = in-game days (agent playtest 2026-11-27)."
    }
    CustomStringTableItem "{709C605E471A9159}" {
     Id "LF-LicenceBarred_Generic"
     Target_en_us "You cannot get this license for %1 days."
     Comment "Toast to the arrested player who did not hold the licence the conviction bars. %1 = in-game days."
    }
    CustomStringTableItem "{7782A81CDE1A5199}" {
     Id "LF-LicenceCounter_Applied"
     Target_en_us "Application taken. Your permit will be ready in about %1 hours. Come back to this counter to collect it."
     Comment "Shop counter licence row: the fee was paid and the permit application filed. %1 = in-game hours until collection."
    }
    CustomStringTableItem "{FE21416A8F87EE0C}" {
     Id "LF-LicenceCounter_Closed"
     Target_en_us "This counter cannot issue permits right now."
     Comment "Shop counter licence row: an internal failure; the server log names it."
    }
    CustomStringTableItem "{C701799D84A17BED}" {
     Id "LF-LicenceCounter_Issued"
     Target_en_us "Here is your firearm permit. This counter will now sell you firearms."
     Comment "Shop counter licence row: the permit was handed over at the gun store counter."
    }
    CustomStringTableItem "{97F1EFC77490B5D8}" {
     Id "LF-LicenceDesk_Action"
     Target_en_us "Apply for a firearm permit"
     Comment "Action on the licence desk: apply for, ask after, or collect the firearm permit (#1766)."
    }
    CustomStringTableItem "{E9A7C4196D8A1F1A}" {
     Id "LF-LicenceDesk_AlreadyHeld"
     Target_en_us "You already hold a firearm permit."
     Comment "Licence desk: the player already has the permit."
    }
    CustomStringTableItem "{619FB3011CA41817}" {
     Id "LF-LicenceDesk_Applied"
     Target_en_us "Application taken. Your permit will be ready in about %1 hours. Come back to this desk to collect it."
     Comment "Licence desk: the fee was paid and the application filed. %1 = in-game hours until collection."
    }
    CustomStringTableItem "{95873A41DBCA4881}" {
     Id "LF-LicenceDesk_Banned"
     Target_en_us "Refused: your firearm permit was seized. Come back in %1 days."
     Comment "Licence desk refusal: a conviction seized the permit and its ban has not ended (#3432). %1 = in-game days left."
    }
    CustomStringTableItem "{F6A22E21A8F71D11}" {
     Id "LF-LicenceDesk_Closed"
     Target_en_us "The license desk cannot help you right now."
     Comment "Licence desk: an internal failure; the server log names it."
    }
    CustomStringTableItem "{7E1C4A90D35B28F6}" {
     Id "LF-LicenceDesk_Convicted"
     Target_en_us "Refused: you were convicted too recently."
     Comment "Licence desk refusal: the player has a conviction within the configured look-back (#1995)."
    }
    CustomStringTableItem "{04D482BE05C4CD10}" {
     Id "LF-LicenceDesk_DrivingAction"
     Target_en_us "Buy a driver's license"
     Comment "Action on the licence desk: buy the driving licence at once for the fee in cash (#3128)."
    }
    CustomStringTableItem "{DB194A67205192B5}" {
     Id "LF-LicenceDesk_DrivingAlreadyHeld"
     Target_en_us "You already hold a driver's license."
     Comment "Licence desk: the player already has the driving licence (#3128)."
    }
    CustomStringTableItem "{4A9DFB45430F1EEF}" {
     Id "LF-LicenceDesk_DrivingBanned"
     Target_en_us "Refused: your driver's license was seized. Come back in %1 days."
     Comment "Licence desk refusal: a conviction seized the driving licence and its ban has not ended (#3432). %1 = in-game days left."
    }
    CustomStringTableItem "{34BBF90BAF789D18}" {
     Id "LF-LicenceDesk_DrivingIssued"
     Target_en_us "Here is your driver's license."
     Comment "Licence desk: the driving licence is handed over (#3128)."
    }
    CustomStringTableItem "{BD0E7DC7F5624449}" {
     Id "LF-LicenceDesk_DrivingNotAllowed"
     Target_en_us "Refused: you do not meet the driver's license requirements."
     Comment "Licence desk refusal for the driving licence: the licence gate (system off, whitelist or job level) (#3128)."
    }
    CustomStringTableItem "{A23456CB32ABE45B}" {
     Id "LF-LicenceDesk_Issued"
     Target_en_us "Here is your firearm permit. A licensed gun counter will now serve you."
     Comment "Licence desk: the permit is handed over."
    }
    CustomStringTableItem "{CA534EEB7372271B}" {
     Id "LF-LicenceDesk_NoCash"
     Target_en_us "Refused: the fee is %1, and your bank account and pockets hold less."
     Comment "Licence desk refusal: the bank account plus carried cash is short of the fee. %1 = the fee, formatted."
    }
    CustomStringTableItem "{4CCA58108F1D9635}" {
     Id "LF-LicenceDesk_NotAllowed"
     Target_en_us "Refused: you do not meet the permit's requirements."
     Comment "Licence desk refusal: the licence's own gate (system off, whitelist or job level)."
    }
    CustomStringTableItem "{2B4D41863E39B001}" {
     Id "LF-LicenceDesk_OpenCrime"
     Target_en_us "Refused: there is an open crime report against you."
     Comment "Licence desk refusal: the player has an open crime report."
    }
    CustomStringTableItem "{038334177B74D9AA}" {
     Id "LF-LicenceDesk_RevokeAction"
     Target_en_us "Revoke firearm permit"
     Comment "Player interaction for an on-duty officer facing a permit holder (#1766)."
    }
    CustomStringTableItem "{52621FF26C08BFD3}" {
     Id "LF-LicenceDesk_Revoked"
     Target_en_us "The {police} revoked your firearm permit."
     Comment "Toast to the player whose permit an officer revoked."
    }
    CustomStringTableItem "{3A50CE3C5AB1EC75}" {
     Id "LF-LicenceDesk_RevokedOfficer"
     Target_en_us "Firearm permit revoked."
     Comment "Toast to the officer after revoking a permit."
    }
    CustomStringTableItem "{7F3E30C72AFD4269}" {
     Id "LF-LicenceDesk_Title"
     Target_en_us "License desk"
     Comment "Notification title for every licence desk answer (#1766)."
    }
    CustomStringTableItem "{CAE386363D4BEF98}" {
     Id "LF-LicenceDesk_Waiting"
     Target_en_us "Your permit is not ready yet. Come back in about %1 hours."
     Comment "Licence desk: the application is still being processed. %1 = in-game hours left."
    }
    CustomStringTableItem "{9BA33F2D2CD65A73}" {
     Id "LF-LicenceDesk_Wanted"
     Target_en_us "Refused: you're wanted by the {police}."
     Comment "Licence desk refusal: the player's wanted level is above zero."
    }
    CustomStringTableItem "{A642D5DB4145E7D9}" {
     Id "LF-LicenceSeized_Driving"
     Target_en_us "Your driver's license has been seized for %1 days."
     Comment "Toast to the arrested player: a conviction took the driving licence (#3432). %1 = ban in in-game days."
    }
    CustomStringTableItem "{D92AB6624E66E7E3}" {
     Id "LF-LicenceSeized_Firearm"
     Target_en_us "Your firearm permit has been seized for %1 days."
     Comment "Toast to the arrested player: a conviction took the firearm permit (#3432). %1 = ban in in-game days."
    }
    CustomStringTableItem "{B409C651C883E235}" {
     Id "LF-LicenceSeized_Generic"
     Target_en_us "A license of yours has been seized for %1 days."
     Comment "Toast to the arrested player: a conviction took a licence with no line of its own (#3432). %1 = ban in in-game days."
    }
    CustomStringTableItem "{190BA98B192ED320}" {
     Id "LF-LicenceSeized_Plain"
     Target_en_us "A license of yours has been seized."
     Comment "Toast to the arrested player: a conviction took a licence with no ban period (#3432)."
    }
    CustomStringTableItem "{AA51A5C85F69418F}" {
     Id "LF-LicenceWhere_DRIVING_LICENCE"
     Target_en_us "The license desk"
     Comment "Where the driving licence is sold, for a refusal (#3150). Neutral core text; a pack row with the same Id names the place."
    }
    CustomStringTableItem "{385688993BAD1A12}" {
     Id "LF-LicenceWhere_Default"
     Target_en_us "Ask at the license desk."
     Comment "Refusal tail for a licence with no where text of its own (#3150)."
    }
    CustomStringTableItem "{3B2EC279E93EE312}" {
     Id "LF-LicenceWhere_FIREARM_PERMIT"
     Target_en_us "The license desk"
     Comment "Where the firearm permit is sold, for a refusal (#3150). Neutral core text; the pack's row with the same Id names the place."
    }
    CustomStringTableItem "{4F0F698E82FCFE93}" {
     Id "LF-LicenceWhere_Job"
     Target_en_us "Earn it through %1 work."
     Comment "Refusal tail for a job licence (#3150): %1 is the job's name; never a level."
    }
    CustomStringTableItem "{C31BB47A38A4999B}" {
     Id "LF-LicenceWhere_Sold"
     Target_en_us "%1 sells it for %2."
     Comment "Refusal tail (#3150): %1 is the place (LF-LicenceWhere_<TYPE>), %2 the fee, e.g. 'The licence desk sells it for $250.'"
    }
    CustomStringTableItem "{C8CAEB5DF24A1E79}" {
     Id "LF-License_CriminalBlackmarket_Desc"
     Target_en_us "You know someone in the back room: illegal weapons, vehicles and clothing."
     Target_es_es "Conoces a alguien en el mercado negro: armas, vehículos y ropa ilegales."
     Comment "Underground skill description"
    }
    CustomStringTableItem "{02105F9797802A3E}" {
     Id "LF-License_CriminalBlackmarket_Name"
     Target_en_us "Back-room contact"
     Target_es_es "Contacto del mercado negro"
     Comment "Underground skill name: access to the black market"
    }
    CustomStringTableItem "{AEC9683418ADA5E9}" {
     Id "LF-License_CriminalClothes_Desc"
     Target_en_us "Lets you buy exclusive clothing in the back room. Requires a back-room contact."
     Target_es_es "Permite comprar ropa exclusiva en el mercado negro. Requiere un contacto del mercado negro."
     Comment "Underground skill description"
    }
    CustomStringTableItem "{D54B837891C0BD0B}" {
     Id "LF-License_CriminalClothes_Name"
     Target_en_us "Back-room tailor's client"
     Target_es_es "Cliente del sastre clandestino"
     Comment "Underground skill name: exclusive black-market clothing"
    }
    CustomStringTableItem "{19A2ACB62354FA15}" {
     Id "LF-License_CriminalRobbery_Desc"
     Target_en_us "You know how to rob shops and other premises. The police can arrest you."
     Target_es_es "Sabes robar tiendas y otros locales. La policía puede arrestarte."
     Comment "Underground skill description"
    }
    CustomStringTableItem "{45C9A748A0EC27AC}" {
     Id "LF-License_CriminalRobbery_Name"
     Target_en_us "Professional thief"
     Target_es_es "Ladrón profesional"
     Comment "Underground skill name: robbery"
    }
    CustomStringTableItem "{05A75882D73C7AA8}" {
     Id "LF-License_CriminalVehicles_Desc"
     Target_en_us "Lets you buy modified or stolen vehicles. Requires a back-room contact."
     Target_es_es "Permite comprar vehículos modificados o robados. Requiere un contacto del mercado negro."
     Comment "Underground skill description"
    }
    CustomStringTableItem "{5EF93ACC4BD2B30E}" {
     Id "LF-License_CriminalVehicles_Name"
     Target_en_us "Vehicle smuggler"
     Target_es_es "Contrabandista de vehículos"
     Comment "Underground skill name: illegal vehicles"
    }
    CustomStringTableItem "{2EDB9E69E8E040F3}" {
     Id "LF-License_CriminalWeapons_Desc"
     Target_en_us "Lets you buy illegal weapons in the back room. Requires a back-room contact."
     Target_es_es "Permite comprar armas ilegales en el mercado negro. Requiere un contacto del mercado negro."
     Comment "Underground skill description"
    }
    CustomStringTableItem "{7FAA4878EC1B10D1}" {
     Id "LF-License_CriminalWeapons_Name"
     Target_en_us "Arms trafficker"
     Target_es_es "Traficante de armas"
     Comment "Underground skill name: illegal weapons"
    }
    CustomStringTableItem "{ECE42E82F73C04B0}" {
     Id "LF-License_CriminalWeed_Desc"
     Target_en_us "You know how to grow and harvest cannabis. Illegal and risky."
     Target_es_es "Sabes cultivar y cosechar marihuana. Ilegal y arriesgado."
     Comment "Underground skill description"
    }
    CustomStringTableItem "{9947E3456A8EF3DE}" {
     Id "LF-License_CriminalWeed_Name"
     Target_en_us "Cannabis grower"
     Target_es_es "Cultivador de marihuana"
     Comment "Underground skill name: growing cannabis"
    }
    CustomStringTableItem "{AFC2B3AB51D2DEA7}" {
     Id "LF-License_Driving_Desc"
     Target_en_us "Lets you drive on public roads."
     Comment "Licence description: the driving licence (#3128)."
    }
    CustomStringTableItem "{B45A8C5B2A8A570E}" {
     Id "LF-License_Driving_Name"
     Target_en_us "Driver's license"
     Comment "Licence name: the driving licence sold at the licence desk (#3128)."
    }
    CustomStringTableItem "{C8B554017BA55A32}" {
     Id "LF-License_FarmerApple_Desc"
     Target_en_us "Permits picking apples in the orchards. Farm work only."
     Target_es_es "Permite recolectar manzanas en los huertos. Solo para trabajo agrícola."
     Comment "Licence description"
    }
    CustomStringTableItem "{F8F02CBA407DB562}" {
     Id "LF-License_FarmerApple_Name"
     Target_en_us "Orchard hand"
     Target_es_es "Hortelano"
     Comment "Licence name: apple picking"
    }
    CustomStringTableItem "{1921CA724A9210CF}" {
     Id "LF-License_FarmerField_Desc"
     Target_en_us "Starting permit for farm work. Lets you gather crops by hand."
     Target_es_es "Permiso inicial de trabajo agrícola. Permite recoger cultivos a mano."
     Comment "Licence description"
    }
    CustomStringTableItem "{33B679F9679C9184}" {
     Id "LF-License_FarmerField_Name"
     Target_en_us "Field hand"
     Target_es_es "Jornalero agrícola"
     Comment "Licence name: starting farm permit"
    }
    CustomStringTableItem "{DB482F762BCADC84}" {
     Id "LF-License_FarmerPlum_Desc"
     Target_en_us "Permits picking plums. Farm work only."
     Target_es_es "Permite recolectar ciruelas. Solo para trabajo agrícola."
     Comment "Licence description"
    }
    CustomStringTableItem "{907E758E2A75E717}" {
     Id "LF-License_FarmerPlum_Name"
     Target_en_us "Plum picker"
     Target_es_es "Recolector de ciruelas"
     Comment "Licence name: plum picking"
    }
    CustomStringTableItem "{9D0A14CCECAD0FA0}" {
     Id "LF-License_FarmerSpeed_Desc"
     Target_en_us "Years in the fields: you pick fruit and draw water twice as fast. Farm work only."
     Target_es_es "Años en el campo: recolectas fruta y agua el doble de rápido. Solo para trabajo agrícola."
     Comment "Licence description"
    }
    CustomStringTableItem "{6B8613BCC510B24D}" {
     Id "LF-License_FarmerSpeed_Name"
     Target_en_us "Seasoned field hand"
     Target_es_es "Peón experimentado"
     Comment "Licence name: faster farm work from experience"
    }
    CustomStringTableItem "{16307C311176370F}" {
     Id "LF-License_FarmerWater_Desc"
     Target_en_us "Permits drawing clean water. Farm work only."
     Target_es_es "Permite recoger agua limpia. Solo para trabajo agrícola."
     Comment "Licence description"
    }
    CustomStringTableItem "{5439217A822B0A98}" {
     Id "LF-License_FarmerWater_Name"
     Target_en_us "Water collection permit"
     Target_es_es "Permiso de recogida de agua"
     Comment "Licence name: drawing clean water"
    }
    CustomStringTableItem "{4FB1382492A8D697}" {
     Id "LF-License_FirearmPermit_Desc"
     Target_en_us "Buy and carry a handgun from a licensed gun counter."
     Comment "Licence description for the firearm permit (#988)."
    }
    CustomStringTableItem "{9F162C71E3737F75}" {
     Id "LF-License_FirearmPermit_Name"
     Target_en_us "Firearm permit"
     Comment "Licence name: lets a civilian buy and carry a handgun from a licensed gun counter (#988)."
    }
    CustomStringTableItem "{942A472691F6510B}" {
     Id "LF-License_Granted"
     Target_en_us "Issued: %1. %2"
     Target_es_es "Expedida: %1. %2"
     Comment "Hint text when a licence is granted. %1 = licence name, %2 = licence description"
    }
    CustomStringTableItem "{72CC7BD2B9986C3B}" {
     Id "LF-License_Granted_Title"
     Target_en_us "License issued"
     Target_es_es "Licencia expedida"
     Comment "Hint title when a licence is granted"
    }
    CustomStringTableItem "{E61C0E0A8EDB2204}" {
     Id "LF-License_LumberjackBasic_Desc"
     Target_en_us "Permits felling trees with an axe. Forestry work only."
     Target_es_es "Permite talar árboles con hacha. Solo para trabajo forestal."
     Comment "Licence description"
    }
    CustomStringTableItem "{963225B9757EABD9}" {
     Id "LF-License_LumberjackBasic_Name"
     Target_en_us "Felling permit"
     Target_es_es "Permiso de tala"
     Comment "Licence name: tree felling"
    }
    CustomStringTableItem "{B70475822D8E7065}" {
     Id "LF-License_LumberjackProcess_Desc"
     Target_en_us "Permits sawing logs into planks at the sawmill. Forestry work only."
     Target_es_es "Permite convertir troncos en tablones en el aserradero. Solo para trabajo forestal."
     Comment "Licence description"
    }
    CustomStringTableItem "{0425EDE7B909E287}" {
     Id "LF-License_LumberjackProcess_Name"
     Target_en_us "Sawmill hand"
     Target_es_es "Operario de aserradero"
     Comment "Licence name: sawmill work"
    }
    CustomStringTableItem "{799448B54F0FA216}" {
     Id "LF-License_LumberjackSpeed_Desc"
     Target_en_us "Years in the forest: you fell trees twice as fast. Forestry work only."
     Target_es_es "Años en el bosque: talas árboles el doble de rápido. Solo para trabajo forestal."
     Comment "Licence description"
    }
    CustomStringTableItem "{9310A55A29C5AB39}" {
     Id "LF-License_LumberjackSpeed_Name"
     Target_en_us "Seasoned logger"
     Target_es_es "Leñador experimentado"
     Comment "Licence name: faster felling from experience"
    }
    CustomStringTableItem "{3D7CB03E481166E3}" {
     Id "LF-License_MedicAccess_Desc"
     Target_en_us "Official ambulance service credential. Required to go on duty."
     Target_es_es "Acreditación oficial del servicio de ambulancias. Necesaria para entrar de servicio."
     Comment "Licence description"
    }
    CustomStringTableItem "{8E937DCF7ED8F47C}" {
     Id "LF-License_MedicAccess_Name"
     Target_en_us "Ambulance service ID"
     Target_es_es "Credencial del servicio de ambulancias"
     Comment "Licence name: ambulance service credential"
    }
    CustomStringTableItem "{A4EB0844DD10A6FB}" {
     Id "LF-License_MedicAdvanced_Desc"
     Target_en_us "Qualifies you for advanced treatment of the injured. Ambulance service only."
     Target_es_es "Te habilita para tratamientos avanzados de los heridos. Solo para el servicio de ambulancias."
     Comment "Licence description"
    }
    CustomStringTableItem "{6D4FFAC95B90D728}" {
     Id "LF-License_MedicAdvanced_Name"
     Target_en_us "Advanced first aid certificate"
     Target_es_es "Certificado de primeros auxilios avanzados"
     Comment "Licence name: advanced treatment"
    }
    CustomStringTableItem "{9E699328091A399D}" {
     Id "LF-License_MedicBasic_Desc"
     Target_en_us "Qualifies you to treat the injured. Ambulance service only."
     Target_es_es "Te habilita para atender a los heridos. Solo para el servicio de ambulancias."
     Comment "Licence description"
    }
    CustomStringTableItem "{10272FAC97B59D22}" {
     Id "LF-License_MedicBasic_Name"
     Target_en_us "First aid certificate"
     Target_es_es "Certificado de primeros auxilios"
     Comment "Licence name: basic treatment"
    }
    CustomStringTableItem "{B3A142A68954DFD4}" {
     Id "LF-License_MedicRevive_Desc"
     Target_en_us "Qualifies you to resuscitate the unconscious. Ambulance service only."
     Target_es_es "Te habilita para reanimar a personas inconscientes. Solo para el servicio de ambulancias."
     Comment "Licence description"
    }
    CustomStringTableItem "{E25FC9B920D69E5B}" {
     Id "LF-License_MedicRevive_Name"
     Target_en_us "Resuscitation certificate"
     Target_es_es "Certificado de reanimación"
     Comment "Licence name: reviving the unconscious"
    }
    CustomStringTableItem "{B9758C07B8B9645C}" {
     Id "LF-License_MinerCoal_Desc"
     Target_en_us "Permits cutting coal with a pick. Mine work only."
     Target_es_es "Permite extraer carbón con pico. Solo para trabajo de mina."
     Comment "Licence description"
    }
    CustomStringTableItem "{D07532E7830E5DA7}" {
     Id "LF-License_MinerCoal_Name"
     Target_en_us "Coal face permit"
     Target_es_es "Permiso de mina de carbón"
     Comment "Licence name: coal mining"
    }
    CustomStringTableItem "{398537ABA9F82833}" {
     Id "LF-License_MinerCopper_Desc"
     Target_en_us "Permits cutting copper ore with a pick, for smelting into ingots. Mine work only."
     Target_es_es "Permite extraer cobre con pico para fundirlo en lingotes. Solo para trabajo de mina."
     Comment "Licence description"
    }
    CustomStringTableItem "{51B16D9C30945CED}" {
     Id "LF-License_MinerCopper_Name"
     Target_en_us "Copper seam permit"
     Target_es_es "Permiso de veta de cobre"
     Comment "Licence name: copper mining"
    }
    CustomStringTableItem "{A98C4D4518CD4411}" {
     Id "LF-License_MinerGold_Desc"
     Target_en_us "Permits cutting gold ore with a pick, for smelting into high-value ingots. Mine work only."
     Target_es_es "Permite extraer oro con pico para fundirlo en lingotes de alto valor. Solo para trabajo de mina."
     Comment "Licence description"
    }
    CustomStringTableItem "{C0B66A5D6C1156ED}" {
     Id "LF-License_MinerGold_Name"
     Target_en_us "Gold seam permit"
     Target_es_es "Permiso de veta de oro"
     Comment "Licence name: gold mining"
    }
    CustomStringTableItem "{344D05FF039E5DB8}" {
     Id "LF-License_MinerIron_Desc"
     Target_en_us "Permits mining iron with a pick. Mine work only."
     Target_es_es "Permite extraer hierro con pico. Solo para trabajo de mina."
     Comment "Licence description"
    }
    CustomStringTableItem "{4B7BC1922CC8F9E1}" {
     Id "LF-License_MinerIron_Name"
     Target_en_us "Iron seam permit"
     Target_es_es "Permiso de veta de hierro"
     Comment "Licence name: iron mining"
    }
    CustomStringTableItem "{F5405603934A1084}" {
     Id "LF-License_MinerSpeed_Desc"
     Target_en_us "Years underground: you cut any ore twice as fast. Mine work only."
     Target_es_es "Años bajo tierra: extraes cualquier mineral el doble de rápido. Solo para trabajo de mina."
     Comment "Licence description"
    }
    CustomStringTableItem "{7CECA66DA479A94D}" {
     Id "LF-License_MinerSpeed_Name"
     Target_en_us "Seasoned miner"
     Target_es_es "Minero experimentado"
     Comment "Licence name: faster mining from experience"
    }
    CustomStringTableItem "{BDE1051C590C0420}" {
     Id "LF-License_PoliceAccess_Desc"
     Target_en_us "Official police credential. Required to go on duty."
     Target_es_es "Acreditación oficial de policía. Necesaria para entrar de servicio."
     Comment "Licence description"
    }
    CustomStringTableItem "{F5DCC2DF5B1749C9}" {
     Id "LF-License_PoliceAccess_Name"
     Target_en_us "Police warrant card"
     Target_es_es "Placa policial"
     Comment "Licence name: police credential"
    }
    CustomStringTableItem "{D4F41D86072924D4}" {
     Id "LF-License_PoliceArrest_Desc"
     Target_en_us "Authorizes arrests. Police officers on the roster only."
     Target_es_es "Autoriza a realizar arrestos. Solo para policías en plantilla."
     Comment "Licence description"
    }
    CustomStringTableItem "{C73088E5160767C2}" {
     Id "LF-License_PoliceArrest_Name"
     Target_en_us "Arrest powers"
     Target_es_es "Facultad de arresto"
     Comment "Licence name: authority to arrest"
    }
    CustomStringTableItem "{C5AA33FDAE156EF8}" {
     Id "LF-License_PoliceFine_Desc"
     Target_en_us "Authorizes issuing fines. Police officers on the roster only."
     Target_es_es "Autoriza a imponer multas. Solo para policías en plantilla."
     Comment "Licence description"
    }
    CustomStringTableItem "{44CBDAFCAE506445}" {
     Id "LF-License_PoliceFine_Name"
     Target_en_us "Fixed penalty powers"
     Target_es_es "Facultad de multar"
     Comment "Licence name: authority to fine"
    }
    CustomStringTableItem "{8D66BEC61FE33397}" {
     Id "LF-License_PoliceSearch_Desc"
     Target_en_us "Authorizes searching suspects. Police officers on the roster only."
     Target_es_es "Autoriza a registrar a sospechosos. Solo para policías en plantilla."
     Comment "Licence description"
    }
    CustomStringTableItem "{B1588D6DB4D51DBF}" {
     Id "LF-License_PoliceSearch_Name"
     Target_en_us "Search powers"
     Target_es_es "Facultad de registro"
     Comment "Licence name: authority to search"
    }
    CustomStringTableItem "{61B7DF5E288DB590}" {
     Id "LF-License_Refused_Title"
     Target_en_us "Not yet"
     Comment "Job-level gate refusal in words, never a number (#2700)."
    }
    CustomStringTableItem "{78705B86BC3B43C0}" {
     Id "LF-License_Unemployed_Desc"
     Target_en_us "No current employment."
     Target_es_es "Sin trabajo activo."
     Comment "Licence description"
    }
    CustomStringTableItem "{9895E82D79A912D8}" {
     Id "LF-License_Unemployed_Name"
     Target_en_us "Unemployed"
     Target_es_es "Desempleado"
     Comment "Licence name: no employment"
    }
    CustomStringTableItem "{3D9A51E07C2B4F62}" {
     Id "LF-LiveryShop_Camo"
     Target_en_us "Camouflage paint"
     Comment "Bay paint job: a drab green camouflage repaint."
    }
    CustomStringTableItem "{B605EF2446246A47}" {
     Id "LF-LiveryShop_Done"
     Target_en_us "Fresh paint. Your car is waiting on the pad."
     Comment "The respray or livery was paid for and the car came back in the new paint."
    }
    CustomStringTableItem "{9EA5DE59135EED28}" {
     Id "LF-LiveryShop_Hint"
     Target_en_us "Paint bay: park your own car on the pad"
     Comment "Action row beside the paint bay pad while the reader has no car of theirs on it."
    }
    CustomStringTableItem "{4B4F10AFC83A6950}" {
     Id "LF-LiveryShop_NoCash"
     Target_en_us "You do not have enough cash on you for this paint job."
     Comment "Respray refused: the owner carries less cash than the price."
    }
    CustomStringTableItem "{216DFEE917592D1F}" {
     Id "LF-LiveryShop_NoOption"
     Target_en_us "That paint job is not offered for this car."
     Comment "Respray refused: the chosen paint or livery does not fit this model."
    }
    CustomStringTableItem "{99F69992AB7041A6}" {
     Id "LF-LiveryShop_NotInBay"
     Target_en_us "Drive the car into the paint bay first."
     Comment "Respray refused: the owner's car is not standing in the mechanic's bay."
    }
    CustomStringTableItem "{3D9A51E07C2B4F61}" {
     Id "LF-LiveryShop_Police"
     Target_en_us "{police} livery"
     Comment "Bay paint job: the department's white-and-navy repaint, sold to police only."
    }
    CustomStringTableItem "{669C1BF76FE6381B}" {
     Id "LF-LiveryShop_Row"
     Target_en_us "Paint: %1 ({currency:sym}%2)"
     Comment "Action row on the paint bay: %1 is the paint or livery name, %2 the price."
    }
    CustomStringTableItem "{B1DCC57A7522779C}" {
     Id "LF-LiveryShop_Title"
     Target_en_us "Paint bay"
     Comment "Toast title for the respray bay."
    }
    CustomStringTableItem "{C4C7B2C8203E90BD}" {
     Id "LF-LiveryShop_WrongJob"
     Target_en_us "That is a service livery. Only that department can order it."
     Comment "Respray refused: a police or ambulance livery for someone without the job."
    }
    CustomStringTableItem "{E767EB7F650628AD}" {
     Id "LF-LockerBank_AlreadyRented"
     Target_en_us "You already rent a locker here."
     Comment "Toast: a second rent at the same locker bank is refused."
    }
    CustomStringTableItem "{0D4C2B7A9E61F358}" {
     Id "LF-LockerBank_CannotAfford"
     Target_en_us "You do not have the rent in cash."
     Comment "Toast or greyed prompt: the player carries less cash than the locker rent."
    }
    CustomStringTableItem "{B15C8F3A62D7E049}" {
     Id "LF-LockerBank_NotRenter"
     Target_en_us "You do not rent a locker here."
     Comment "Toast: the character tried to open a locker at a bank where they hold no rental."
    }
    CustomStringTableItem "{7D2E41B8C09A53F6}" {
     Id "LF-LockerBank_Open"
     Target_en_us "Open your locker"
     Comment "Action on a locker bank: open the renter's own locker contents."
    }
    CustomStringTableItem "{93E1A6C04B7D2F85}" {
     Id "LF-LockerBank_OutOfReach"
     Target_en_us "Stand at the lockers to rent one."
     Comment "Toast: the server found no locker bank within reach of the player."
    }
    CustomStringTableItem "{4F27D8B1C0A3E96D}" {
     Id "LF-LockerBank_Refused"
     Target_en_us "The locker could not be rented."
     Comment "Toast: the rent was refused for a reason the player cannot fix (logged)."
    }
    CustomStringTableItem "{5B4F91955B355624}" {
     Id "LF-LockerBank_Rent"
     Target_en_us "Rent a locker"
     Comment "Action on a locker bank: pay the rent for one locker here."
    }
    CustomStringTableItem "{5166BCD3DA9EC12C}" {
     Id "LF-LockerBank_Rented"
     Target_en_us "You rented a locker here."
     Comment "Toast: the rent was paid and the locker at this bank is the player's."
    }
    CustomStringTableItem "{6A90D3BA614FE639}" {
     Id "LF-LockerBank_Title"
     Target_en_us "Lockers"
     Comment "Title of the locker bank prompt and toast."
    }
    CustomStringTableItem "{606F8E5D11807BC8}" {
     Id "LF-Locker_Cases"
     Target_en_us "Case file: %1"
     Comment "Toast: newest logged bags; %1 is the list."
    }
    CustomStringTableItem "{155ED171C52FF11B}" {
     Id "LF-Locker_Empty"
     Target_en_us "The case file is empty."
     Comment "Toast: nothing is logged in the locker."
    }
    CustomStringTableItem "{8B98128F11F74501}" {
     Id "LF-Locker_Log_Action"
     Target_en_us "Log evidence bags"
     Comment "Action: log carried evidence bags."
    }
    CustomStringTableItem "{1C43A9F024D5E4F2}" {
     Id "LF-Locker_Logged"
     Target_en_us "Logged %1 evidence bag(s) against their reports."
     Comment "Toast: bags logged in the locker; %1 is the count."
    }
    CustomStringTableItem "{EF5331F445A7CC64}" {
     Id "LF-Locker_Name"
     Target_en_us "Evidence locker"
     Comment "Name of the evidence locker."
    }
    CustomStringTableItem "{FAD52119B88896AE}" {
     Id "LF-Locker_NoBags"
     Target_en_us "You carry no evidence bag to log."
     Comment "Toast: nothing to log."
    }
    CustomStringTableItem "{F08CF5A240E91728}" {
     Id "LF-Locker_Raid_Action"
     Target_en_us "Burn the case file"
     Comment "Action: destroy the logged evidence of a forced locker."
    }
    CustomStringTableItem "{2863ED99FCE23578}" {
     Id "LF-Locker_Raided"
     Target_en_us "The case file is burned: %1 bag(s) destroyed."
     Comment "Toast: a raid destroyed the logged bags; %1 is the count."
    }
    CustomStringTableItem "{BD207ABDEF5524C1}" {
     Id "LF-Locker_Read_Action"
     Target_en_us "Read case file"
     Comment "Action: read the case file."
    }
    CustomStringTableItem "{65B97D3424738198}" {
     Id "LF-Locker_Title"
     Target_en_us "Evidence locker"
     Comment "Toast title for the evidence locker."
    }
    CustomStringTableItem "{B9EA3C536FECFC95}" {
     Id "LF-Lockpick_Action"
     Target_en_us "Pick the lock"
     Comment "Action: work a locked door with a lockpick (#411)."
    }
    CustomStringTableItem "{50027139FC87315E}" {
     Id "LF-Lockpick_Busy"
     Target_en_us "Someone is already working this lock"
     Comment "Refusal: an attempt is running (#411)."
    }
    CustomStringTableItem "{7D8C5DBC163257A8}" {
     Id "LF-Lockpick_Cancelled"
     Target_en_us "You stepped away from the lock"
     Comment "Toast: the attempt was dropped (#411)."
    }
    CustomStringTableItem "{0887037FED6A5F0F}" {
     Id "LF-Lockpick_Failed"
     Target_en_us "The pick slips. Try again."
     Comment "Toast: the attempt failed (#411)."
    }
    CustomStringTableItem "{340833241E6EC66D}" {
     Id "LF-Lockpick_NeedPick"
     Target_en_us "You need a lockpick"
     Comment "Refusal: no lockpick carried (#411)."
    }
    CustomStringTableItem "{725DAC6044490CDB}" {
     Id "LF-Lockpick_NotLocked"
     Target_en_us "It is not locked"
     Comment "Refusal: the door is already unlocked (#411)."
    }
    CustomStringTableItem "{FD07CC0DE719CAA6}" {
     Id "LF-Lockpick_NotPickable"
     Target_en_us "This lock cannot be picked"
     Comment "Refusal: the target is not pickable (#411)."
    }
    CustomStringTableItem "{2D0C66EA62BED08B}" {
     Id "LF-Lockpick_Opened"
     Target_en_us "The lock gives"
     Comment "Toast: the lock opened (#411)."
    }
    CustomStringTableItem "{06E530ED6D12AC2F}" {
     Id "LF-Lockpick_Snapped"
     Target_en_us "The pick snaps"
     Comment "Toast: the pick snapped (#411)."
    }
    CustomStringTableItem "{06E6A4A3A962339D}" {
     Id "LF-Lockpick_Started"
     Target_en_us "You start working the lock. Stay close."
     Comment "Toast: a pick attempt began (#411)."
    }
    CustomStringTableItem "{B6AE5AF587643C0C}" {
     Id "LF-Lockpick_Title"
     Target_en_us "Lockpick"
     Comment "Toast title for lockpicking (#411)."
    }
    CustomStringTableItem "{F87E1E94DC2EE743}" {
     Id "LF-LogoutBodyTag_Label"
     Target_en_us "Logged out"
     Comment "World-space tag over a logged-out player's body (#1974)."
    }
    CustomStringTableItem "{C2059960A9DF4550}" {
     Id "LF-Loot_Cooldown"
     Target_en_us "Nothing new here yet. Try again in about %1 min."
     Comment "Toast: an emptied container has not refilled yet. %1 = minutes."
    }
    CustomStringTableItem "{0EFBD383AE6DFA08}" {
     Id "LF-Loot_CrateName"
     Target_en_us "Crate"
     Comment "Name of a searchable loot crate in the inventory."
    }
    CustomStringTableItem "{9304B5DAB88FF982}" {
     Id "LF-Loot_Found"
     Target_en_us "You search the crate and find something."
     Comment "Toast after a search filled the container."
    }
    CustomStringTableItem "{D21D64729C6E9008}" {
     Id "LF-Loot_Open"
     Target_en_us "Open crate"
     Comment "Action on a loot container: opens it as normal storage."
    }
    CustomStringTableItem "{A9AB3A8A23789166}" {
     Id "LF-Loot_Search"
     Target_en_us "Search"
     Comment "Action on a loot container: a short hold that fills it from its loot table (#3343)."
    }
    CustomStringTableItem "{292DEA0C67C02DD6}" {
     Id "LF-Loot_StillStocked"
     Target_en_us "There is still something in here."
     Comment "Toast: a search added nothing because the container still holds items."
    }
    CustomStringTableItem "{F4126E9A5B38D07C}" {
     Id "LF-LostAndFound_CannotAfford"
     Target_en_us "You do not have the fee in cash."
     Comment "Toast: the reclaim fee is not in hand; nothing was taken and nothing moved."
    }
    CustomStringTableItem "{0D58B6F3C7A1924E}" {
     Id "LF-LostAndFound_Nothing"
     Target_en_us "Nothing of yours is waiting here."
     Comment "Toast: nothing waits at this site's lost-and-found for the player (a stranger, or already taken)."
    }
    CustomStringTableItem "{67B3D08E2C915FA4}" {
     Id "LF-LostAndFound_OutOfReach"
     Target_en_us "Step closer to the lost-and-found box."
     Comment "Toast: no lost-and-found box within reach."
    }
    CustomStringTableItem "{3E71A5C08D294B6F}" {
     Id "LF-LostAndFound_Reclaim"
     Target_en_us "Reclaim your things (fee)"
     Comment "Action on the lost-and-found box: pay the reclaim fee and take back what a lapsed locker held."
    }
    CustomStringTableItem "{A90C42E7165DB83F}" {
     Id "LF-LostAndFound_Reclaimed"
     Target_en_us "You paid the fee and took your things back."
     Comment "Toast: the reclaim fee was taken and every waiting item moved to the player."
    }
    CustomStringTableItem "{C28E5A1F90D64B37}" {
     Id "LF-LostAndFound_Refused"
     Target_en_us "The lost-and-found could not hand your things over."
     Comment "Toast: the reclaim was refused for another reason; see the log."
    }
    CustomStringTableItem "{19F7C4B6E3A85D02}" {
     Id "LF-LostAndFound_Title"
     Target_en_us "Lost and found"
     Comment "Title of the lost-and-found box prompt and toast."
    }
    CustomStringTableItem "{EBA0820132F44791}" {
     Id "LF-Lot_Browse"
     Target_en_us "Look at the cars"
     Comment "Car lot interaction: opens the lot price sheet."
    }
    CustomStringTableItem "{7C18C714BFDC5DBE}" {
     Id "LF-Lot_Buy"
     Target_en_us "Buy"
     Comment "Car lot row button."
    }
    CustomStringTableItem "{811F406212DFA1D1}" {
     Id "LF-Lot_ChangeLocks"
     Target_en_us "Change the locks"
     Comment "Car lot sheet row and toast title: the owner pays for new locks on their latest car; every older key stops working (#2411)."
    }
    CustomStringTableItem "{8CEDC85FF7E4E4F0}" {
     Id "LF-Lot_ChangeLocksButton"
     Target_en_us "Change"
     Comment "Car lot sheet button on the change-locks row."
    }
    CustomStringTableItem "{F332871C9531979A}" {
     Id "LF-Lot_ChangeLocksRow"
     Target_en_us "Locks: %1"
     Comment "Car lot sheet row: re-key the car with this plate. %1 = the plate."
    }
    CustomStringTableItem "{EEBDBD6504C8B4FF}" {
     Id "LF-Lot_Leave"
     Target_en_us "Leave"
     Comment "Car lot sheet close button."
    }
    CustomStringTableItem "{0FCCB2C68B85D51F}" {
     Id "LF-Lot_LocksChanged"
     Target_en_us "New locks fitted. Your old keys no longer open the car; here is the new one."
     Comment "Toast after the owner changed the locks on their car."
    }
    CustomStringTableItem "{3E2EC9B19D88B571}" {
     Id "LF-Lot_Money"
     Target_en_us "In the bank: %1    Cash in hand: %2"
     Comment "Car lot sheet: the money the reader can pay with. %1 bank, %2 cash."
    }
    CustomStringTableItem "{3A23ABBFC38B6FC4}" {
     Id "LF-Lot_NeedsBank"
     Target_en_us "needs %1"
     Comment "Car lot row for a car the reader cannot afford from the bank. %1 is the price."
    }
    CustomStringTableItem "{3E8956543B8EFAAA}" {
     Id "LF-Lot_NeedsCash"
     Target_en_us "needs %1"
     Comment "Car lot row for a cheap car the reader cannot afford in cash. %1 is the price."
    }
    CustomStringTableItem "{875D23933B55810A}" {
     Id "LF-Lot_NeedsDrivingLicence"
     Target_en_us "Not without a driver's license. The license desk issues it for %1."
     Comment "Car lot refusal when the server asks for a driving licence (#3151). %1 is the fee. Neutral core text; a pack row with the same Id names the place."
    }
    CustomStringTableItem "{B7C2E4F1A93D5680}" {
     Id "LF-Lot_NeedsTruckLicence"
     Target_en_us "Trucks need the truck license first. The license desk runs the truck check for %1."
     Comment "Vehicle lot refusal for a truck without the truck license (#4417). %1 is the truck check fee. Neutral core text."
    }
    CustomStringTableItem "{5C1E7A94D2B03F68}" {
     Id "LF-Lot_RowNeedsTruck"
     Target_en_us "%1 (needs truck license)"
     Comment "Vehicle lot sheet: a truck row says it needs the truck license before Buy (#4417). %1 is the truck's name."
    }
    CustomStringTableItem "{B58D71B1650E8EBB}" {
     Id "LF-Lot_Rule"
     Target_en_us "Cars up to %1 are paid in cash. Dearer cars are paid from your bank: deposit cash at the ATM first."
     Comment "Car lot sheet: the payment rule. %1 is the cash ceiling."
    }
    CustomStringTableItem "{2099A882EC6B2C01}" {
     Id "LF-Lot_Title"
     Target_en_us "Car lot - price list"
     Comment "Car lot sheet title."
    }
    CustomStringTableItem "{D346F69E748E0356}" {
     Id "LF-Lumber_AlreadyFelled"
     Target_en_us "This tree is already down."
     Comment "Toast: felling refused, the tree is felled and has not regrown (#3259)."
    }
    CustomStringTableItem "{C80E5A37B2F94D16}" {
     Id "LF-Lumber_ClassHardwood"
     Target_en_us "hardwood"
     Comment "Wood class word in the fell prompt (#3260)."
    }
    CustomStringTableItem "{2D94B6F8C1A07E53}" {
     Id "LF-Lumber_ClassSoftwood"
     Target_en_us "softwood"
     Comment "Wood class word in the fell prompt (#3260)."
    }
    CustomStringTableItem "{91B7D2E4A6F30C58}" {
     Id "LF-Lumber_ClassTree"
     Target_en_us "tree"
     Comment "Wood class word in the fell prompt when the class is unknown (#3260)."
    }
    CustomStringTableItem "{5F2A8C41D7E36B90}" {
     Id "LF-Lumber_Fell"
     Target_en_us "Fell tree (%1, %2 s)"
     Comment "Prompt of the axe hold action on a map tree (#3260). %1 = wood class, %2 = hold seconds."
    }
    CustomStringTableItem "{A7C3E19B50D84F26}" {
     Id "LF-Lumber_Felled"
     Target_en_us "Tree down. %1 log(s) on the ground."
     Comment "Toast: a map tree was felled (#3260). %1 = logs dropped."
    }
    CustomStringTableItem "{994FBED5D9F12308}" {
     Id "LF-Lumber_NearBuilding"
     Target_en_us "Too close to a building."
     Comment "Toast: felling refused, a building is too near; keeps gardens and avenues standing (#3259)."
    }
    CustomStringTableItem "{DE994BAA5959C133}" {
     Id "LF-Lumber_NearRoad"
     Target_en_us "Too close to the road: it would fall across it."
     Comment "Toast: felling refused, a road or paved surface is too near (#3259)."
    }
    CustomStringTableItem "{61435D35A213695C}" {
     Id "LF-Lumber_NoAxe"
     Target_en_us "You need an axe in your hands."
     Comment "Toast: felling refused, no axe held (#3259)."
    }
    CustomStringTableItem "{39ECA47F906B44F1}" {
     Id "LF-Lumber_NotATree"
     Target_en_us "That can't be felled."
     Comment "Toast: felling refused, a stump, fallen trunk, orchard or other tree that is never felled (#3259)."
    }
    CustomStringTableItem "{483778CCE8D259AA}" {
     Id "LF-Lumber_SafeZone"
     Target_en_us "No felling here."
     Comment "Toast: felling refused inside a safe zone (#3259)."
    }
    CustomStringTableItem "{BC5CB49CBB20C17E}" {
     Id "LF-Lumber_Tired"
     Target_en_us "You're too tired. Catch your breath first."
     Comment "Toast: felling refused, stamina is below what the tree costs (#3259)."
    }
    CustomStringTableItem "{1BD92A70876B9267}" {
     Id "LF-Lumber_TooFar"
     Target_en_us "Get closer to the trunk."
     Comment "Toast: felling refused, the player stands too far from the tree (#3259)."
    }
    CustomStringTableItem "{BB558DD7CB298EBF}" {
     Id "LF-Lumber_TooSmall"
     Target_en_us "Too young to fell. Let it grow."
     Comment "Toast: felling refused, a sapling (#3259)."
    }
    CustomStringTableItem "{3651646B23FCD86B}" {
     Id "LF-Magazine_Taser_Description"
     Target_en_us "One shot for the taser."
     Comment "Inventory description of the taser cartridge."
    }
    CustomStringTableItem "{4EBBF5311DDAD1CF}" {
     Id "LF-Magazine_Taser_Name"
     Target_en_us "Taser cartridge"
     Comment "Inventory name of the taser's single-shot cartridge."
    }
    CustomStringTableItem "{69DF48978FFA6832}" {
     Id "LF-Mannequin_Bought"
     Target_en_us "Bought for %1."
     Comment "Toast after buying a piece off a mannequin. %1 price paid."
    }
    CustomStringTableItem "{D88719E262E2BBFE}" {
     Id "LF-Mannequin_BoughtDropped"
     Target_en_us "Bought for %1. Your pockets are full, so it is at your feet."
     Comment "Toast: bought a piece, no room in the inventory, it was dropped at the buyer. %1 price paid."
    }
    CustomStringTableItem "{E3C98F280138BB07}" {
     Id "LF-Mannequin_BoughtOutfit"
     Target_en_us "Bought the whole outfit for %1."
     Comment "Toast after buying a whole mannequin outfit. %1 price paid."
    }
    CustomStringTableItem "{E9FC2C3F7CA0EF0D}" {
     Id "LF-Mannequin_BoughtOutfitDropped"
     Target_en_us "Bought the whole outfit for %1. What did not fit is at your feet."
     Comment "Toast: bought the outfit, some pieces were dropped at the buyer. %1 price paid."
    }
    CustomStringTableItem "{023966184146C94F}" {
     Id "LF-Mannequin_BoughtPart"
     Target_en_us "Bought part of the outfit for %1; the rest was refused."
     Comment "Toast: the outfit sale stopped part way (a gate refused one piece). %1 price paid."
    }
    CustomStringTableItem "{1525CDCF4C868B0B}" {
     Id "LF-Mannequin_BuyOutfit"
     Target_en_us "Buy whole outfit: %1"
     Comment "Mannequin action: buy every worn piece. %1 total price."
    }
    CustomStringTableItem "{749A44898F468261}" {
     Id "LF-Mannequin_BuyPiece"
     Target_en_us "Buy %1: %2"
     Comment "Mannequin action: buy one worn piece. %1 item name, %2 price."
    }
    CustomStringTableItem "{097B35901C0BFF32}" {
     Id "LF-Mannequin_NoCash"
     Target_en_us "Not enough cash"
     Comment "Mannequin action greyed: the player carries less cash than the price."
    }
    CustomStringTableItem "{F224EE96F400AC18}" {
     Id "LF-Mannequin_OutfitSoldOut"
     Target_en_us "A piece is sold out"
     Comment "Mannequin outfit action greyed: one worn piece is sold out."
    }
    CustomStringTableItem "{CFF4E53D12DE196D}" {
     Id "LF-Mannequin_SoldOut"
     Target_en_us "Sold out"
     Comment "Mannequin action greyed: the store has none of this piece left."
    }
    CustomStringTableItem "{95C19D11A4510149}" {
     Id "LF-Mannequin_Title"
     Target_en_us "Mannequin"
     Comment "Toast title for a purchase off a dressed mannequin."
    }
    CustomStringTableItem "{8E30B6C33C6FD605}" {
     Id "LF-MapAlarm_InProgress"
     Target_en_us "Robbery in progress (alarm raised %1 min ago)"
     Comment "Map place card: this place is being robbed now. %1 = whole minutes since its alarm went off."
    }
    CustomStringTableItem "{DB6032E5F040F5BA}" {
     Id "LF-MapAlarm_InProgressNow"
     Target_en_us "Robbery in progress (alarm just raised)"
     Comment "Map place card: this place is being robbed now; the alarm went off under a minute ago."
    }
    CustomStringTableItem "{C2A51B918A2C343E}" {
     Id "LF-MapAlarm_Key"
     Target_en_us "Alarm: a red ring is a robbery in progress, a gray badge was robbed recently"
     Comment "Map key entry explaining the alarm ring on place badges."
    }
    CustomStringTableItem "{E45E29B5B3493411}" {
     Id "LF-MapAlarm_Recent"
     Target_en_us "Robbed recently"
     Comment "Map place card: this place was robbed a few minutes ago and its alarm has stopped."
    }
    CustomStringTableItem "{CA6AAA75BD284BC8}" {
     Id "LF-MapHelp_Clear"
     Target_en_us "Shift + Left click on the waypoint, or Backspace: clear it"
     Comment "Map help card line, mouse and keyboard."
    }
    CustomStringTableItem "{E2D39593E3B44FE8}" {
     Id "LF-MapHelp_ClearPad"
     Target_en_us "Hold Y + A on the waypoint, or double-tap Y: clear it"
     Comment "Map help card line, gamepad."
    }
    CustomStringTableItem "{E01B227758134106}" {
     Id "LF-MapHelp_Folded"
     Target_en_us "Map help: press F1"
     Comment "Map help card folded to one line, mouse and keyboard."
    }
    CustomStringTableItem "{9BC7965FB084E408}" {
     Id "LF-MapHelp_FoldedPad"
     Target_en_us "Map help: click the left stick"
     Comment "Map help card folded to one line, gamepad."
    }
    CustomStringTableItem "{005BC80EFBA87F74}" {
     Id "LF-MapHelp_Hover"
     Target_en_us "Hover an icon: see its name"
     Comment "Map help card line, mouse and keyboard."
    }
    CustomStringTableItem "{3BBBDEAB72CC5988}" {
     Id "LF-MapHelp_HoverPad"
     Target_en_us "Move the cursor onto an icon: see its name"
     Comment "Map help card line, gamepad (the right stick moves the map cursor)."
    }
    CustomStringTableItem "{7338A2A8E11FC065}" {
     Id "LF-MapHelp_Legend"
     Target_en_us "Key (right edge): what each icon is; hover a row to find them"
     Comment "Map help card line: the Legend tab under Map key on the left edge."
    }
    CustomStringTableItem "{3B4BC9DDE46EB600}" {
     Id "LF-MapHelp_NoWaypoint"
     Target_en_us "No waypoint set"
     Comment "Map help card, waypoint row when none is set."
    }
    CustomStringTableItem "{168BC3FE7884D09D}" {
     Id "LF-MapHelp_Place"
     Target_en_us "Click a place: set a waypoint there"
     Comment "Map help card line, mouse and keyboard."
    }
    CustomStringTableItem "{E482C52EEABBD3E9}" {
     Id "LF-MapHelp_PlacePad"
     Target_en_us "A on a place: set a waypoint there"
     Comment "Map help card line, gamepad."
    }
    CustomStringTableItem "{C34246D97B2CFB82}" {
     Id "LF-MapHelp_Set"
     Target_en_us "Shift + Left click: set a waypoint"
     Comment "Map help card line, mouse and keyboard."
    }
    CustomStringTableItem "{1D544AF20A9433FB}" {
     Id "LF-MapHelp_SetPad"
     Target_en_us "Hold Y + A: set a waypoint"
     Comment "Map help card line, gamepad."
    }
    CustomStringTableItem "{C517F4FC333CBD2E}" {
     Id "LF-MapHelp_Title"
     Target_en_us "How to use the map (F1 hides this)"
     Comment "Map help card title, mouse and keyboard."
    }
    CustomStringTableItem "{A79B20C2E5C018E5}" {
     Id "LF-MapHelp_TitlePad"
     Target_en_us "How to use the map (left stick click hides this)"
     Comment "Map help card title, gamepad."
    }
    CustomStringTableItem "{1AA71BD54941ADF6}" {
     Id "LF-MapHelp_Waypoint"
     Target_en_us "Waypoint: %1"
     Comment "Map help card, waypoint row. %1 = the distance (340 m) or the place and the distance (Cafe, 340 m)."
    }
    CustomStringTableItem "{441F95A67FA9CF85}" {
     Id "LF-MapKey_Alarm"
     Target_en_us "Alarm: robbery"
     Comment "Map key line: a red ring on a badge means a robbery is in progress there (grey badge: robbed recently)."
    }
    CustomStringTableItem "{203B7958738CC593}" {
     Id "LF-MapKey_Run"
     Target_en_us "Job run: hover a stop"
     Comment "Map key line: hovering a badge on a job run draws its stops joined by a dashed line, numbered in order."
    }
    CustomStringTableItem "{4FDA78D6942096A2}" {
     Id "LF-MapKey_Tab"
     Target_en_us "Key: hide"
     Comment "Map key column tab while the column is open: a click folds it."
    }
    CustomStringTableItem "{2D2A708C6C0BDC7C}" {
     Id "LF-MapKey_TabFolded"
     Target_en_us "Key"
     Comment "Map key column tab while folded: a click opens the column of icons."
    }
    CustomStringTableItem "{D920AB80A60A3625}" {
     Id "LF-MapKnown_ArrivalMarked"
     Target_en_us "The clerk pencils %1 places on the square onto your map."
     Comment "Map discovery (#2242): toast at the end of the arrival chain. %1 = places new to the player's map."
    }
    CustomStringTableItem "{13F7361AFE5B4C57}" {
     Id "LF-MapKnown_Title"
     Target_en_us "Your map"
     Comment "Map discovery (#2242): the toast title for the clerk's arrival marks."
    }
    CustomStringTableItem "{FB41DE0AE665439A}" {
     Id "LF-MapLegend_Cat_civic"
     Target_en_us "Civic"
     Comment "Map legend (#2244): category line, civic places."
    }
    CustomStringTableItem "{DB2CEFE2D4AB3134}" {
     Id "LF-MapLegend_Cat_health"
     Target_en_us "Health"
     Comment "Map legend (#2244): category line, clinics and hospital."
    }
    CustomStringTableItem "{1264A12EDAE44B4D}" {
     Id "LF-MapLegend_Cat_law"
     Target_en_us "Law"
     Comment "Map legend (#2244): category line, police and law."
    }
    CustomStringTableItem "{0356E8394B3CCC79}" {
     Id "LF-MapLegend_Cat_shop"
     Target_en_us "Shops"
     Comment "Map legend (#2244): category line, shops."
    }
    CustomStringTableItem "{8C14520EB5B1F9F5}" {
     Id "LF-MapLegend_Cat_story"
     Target_en_us "Story places"
     Comment "Map legend (#2244): category line, story places."
    }
    CustomStringTableItem "{5663C5F74F2B4A2C}" {
     Id "LF-MapLegend_Cat_travel"
     Target_en_us "Travel"
     Comment "Map legend (#2244): category line, taxi and harbour."
    }
    CustomStringTableItem "{AA8F815614FBCA7B}" {
     Id "LF-MapLegend_Cat_underworld"
     Target_en_us "Penciled places"
     Comment "Map legend (#2244): category line, places the player only met and marked."
    }
    CustomStringTableItem "{532D924D534C72E5}" {
     Id "LF-MapLegend_Cat_work"
     Target_en_us "Work"
     Comment "Map legend (#2244): category line, places of work."
    }
    CustomStringTableItem "{BABDE49853D9B663}" {
     Id "LF-MapLegend_Close"
     Target_en_us "Close"
     Comment "Map legend (#2244): closes the sheet."
    }
    CustomStringTableItem "{68E7EBFADC8A8DA6}" {
     Id "LF-MapLegend_Find"
     Target_en_us "Find"
     Comment "Map legend (#2244): the button that runs the search."
    }
    CustomStringTableItem "{4062C7A0E4B4025D}" {
     Id "LF-MapLegend_Go"
     Target_en_us "Go to"
     Comment "Map legend (#2244): button on a search result, pans the map to the place."
    }
    CustomStringTableItem "{21D8FEFAC7CBA51F}" {
     Id "LF-MapLegend_Heading"
     Target_en_us "Show on the map"
     Comment "Map legend (#2244): heading over the category lines."
    }
    CustomStringTableItem "{849CF0B065C6A4FD}" {
     Id "LF-MapLegend_Hide"
     Target_en_us "Hide"
     Comment "Map legend (#2244): button that takes a category off the map."
    }
    CustomStringTableItem "{13D7B2C3585F2511}" {
     Id "LF-MapLegend_NoMatch"
     Target_en_us "No place you know of by that name"
     Comment "Map legend (#2244): shown when a search finds no known place."
    }
    CustomStringTableItem "{969CA1332312A3ED}" {
     Id "LF-MapLegend_Off"
     Target_en_us "Hidden"
     Comment "Map legend (#2244): status of a category left off the map."
    }
    CustomStringTableItem "{300A53CF8CF58594}" {
     Id "LF-MapLegend_On"
     Target_en_us "Shown"
     Comment "Map legend (#2244): status of a category drawn on the map."
    }
    CustomStringTableItem "{2DD3F3DDA0A65424}" {
     Id "LF-MapLegend_Search"
     Target_en_us "Find a place"
     Comment "Map legend (#2244): caption of the search blank."
    }
    CustomStringTableItem "{CE42C02CF49D5038}" {
     Id "LF-MapLegend_Show"
     Target_en_us "Show"
     Comment "Map legend (#2244): button that puts a category back on the map."
    }
    CustomStringTableItem "{EC872DF7663D8669}" {
     Id "LF-MapLegend_Tab"
     Target_en_us "Find a place"
     Comment "Map legend (#2244): the tab on the map edge that opens the key."
    }
    CustomStringTableItem "{9188A58C330DB074}" {
     Id "LF-MapLegend_Title"
     Target_en_us "Map key"
     Comment "Map legend sheet (#2244): the sheet title."
    }
    CustomStringTableItem "{3B7E5C19A2D40F68}" {
     Id "LF-MapMarkWaypoint_Label"
     Target_en_us "Map marker"
     Comment "Map: name under the HUD marker of the waypoint a player's own map marker sets, when the marker has no text of its own."
    }
    CustomStringTableItem "{A903D8336DDE33B0}" {
     Id "LF-MapPlaces_CardClear"
     Target_en_us "Your waypoint. Click again to clear."
     Comment "Map place card, mouse: the waypoint is on this place."
    }
    CustomStringTableItem "{A4B75C962C2E3FDD}" {
     Id "LF-MapPlaces_CardClearPad"
     Target_en_us "Your waypoint. A again to clear."
     Comment "Map place card, gamepad: the waypoint is on this place."
    }
    CustomStringTableItem "{26B1FA3CD62AF095}" {
     Id "LF-MapPlaces_CardCrowded"
     Target_en_us "Also here: %1. Click to zoom in."
     Comment "Map place card for places that share one spot on the map. %1 = the other places, comma separated."
    }
    CustomStringTableItem "{BA2C603B8BA2DEAA}" {
     Id "LF-MapPlaces_CardSet"
     Target_en_us "Click: set waypoint"
     Comment "Map place card under the cursor, mouse: what a click on the place does."
    }
    CustomStringTableItem "{A752450458ACD025}" {
     Id "LF-MapPlaces_CardSetPad"
     Target_en_us "A: set waypoint"
     Comment "Map place card under the cursor, gamepad: what A on the place does."
    }
    CustomStringTableItem "{DE2AC99F6166AB0D}" {
     Id "LF-MapPlaces_ClusterNote"
     Target_en_us "Zoom in for each"
     Comment "Map POI hover card (#2232): the line under a cluster stamp's title, the click zooms in."
    }
    CustomStringTableItem "{FAA5EDA4BA7F60BD}" {
     Id "LF-MapPlaces_ClusterTitle"
     Target_en_us "%1 places"
     Comment "Map POI hover card (#2232): a cluster stamp over several places; %1 is the count."
    }
    CustomStringTableItem "{EB32F79C95C8B87F}" {
     Id "LF-MapPlaces_HeardOf"
     Target_en_us "Heard of, not visited"
     Comment "Map POI hover card (#2232): the line under a faded place the player has only heard of."
    }
    CustomStringTableItem "{898E403E63E0AFCF}" {
     Id "LF-MapPlaces_SetWaypoint"
     Target_en_us "Set waypoint"
     Comment "Map POI hover card (#2232): the button that marks the place as the personal waypoint."
    }
    CustomStringTableItem "{06BAB63766610E3E}" {
     Id "LF-MapPoi_Bank"
     Target_en_us "Bank"
     Comment "Map POI kind name (#2230): the bank."
    }
    CustomStringTableItem "{963CABB6216B6480}" {
     Id "LF-MapPoi_BusStop"
     Target_en_us "Bus stop"
     Comment "Map place name: a bus stop."
    }
    CustomStringTableItem "{9FA312C17AB06C17}" {
     Id "LF-MapPoi_Hospital"
     Target_en_us "Clinic"
     Comment "Map POI kind name (#2230): the hospital or clinic."
    }
    CustomStringTableItem "{912FF601FBAB64D2}" {
     Id "LF-MapPoi_PlotForSale"
     Target_en_us "Plot for sale"
     Comment "Map POI kind name (#2984): a plot sign nobody owns yet; it leaves the map once the plot is bought."
    }
    CustomStringTableItem "{83D5FBD46034ADAD}" {
     Id "LF-MapPoi_PoliceArmoury"
     Target_en_us "{Police} armory"
     Comment "Map POI kind name (#2230): the police armoury, shown to officers only."
    }
    CustomStringTableItem "{02A346FCF31764FE}" {
     Id "LF-MapPoi_PoliceStation"
     Target_en_us "{Police} station"
     Comment "Map POI kind name (#2230): the police station."
    }
    CustomStringTableItem "{B00FE6F8C70CF970}" {
     Id "LF-MapPoi_Workplace"
     Target_en_us "Workplace"
     Comment "Map POI kind name (#2230): a job site."
    }
    CustomStringTableItem "{74C9103828270975}" {
     Id "LF-MapRun_Card"
     Target_en_us "%1: step %2 of %3, next: %4"
     Comment "Map place card on a stop of a job run. %1 = run name (Mining run), %2 = this step, %3 = steps in the run, %4 = the next stop (furnace)."
    }
    CustomStringTableItem "{8E0453D4FDB992D1}" {
     Id "LF-MapRun_CardLast"
     Target_en_us "%1: step %2 of %3, last stop"
     Comment "Map place card on the last stop of a job run. %1 = run name, %2 = this step, %3 = steps in the run."
    }
    CustomStringTableItem "{DC7EEF1A69D6A983}" {
     Id "LF-MapWaypoint_Label"
     Target_en_us "Waypoint"
     Comment "Map: name of the waypoint a player set by right-clicking the map (shown under the marker and by the pencil circle)."
    }
    CustomStringTableItem "{71DFE7C94DAE37F2}" {
     Id "LF-Map_AreaGated"
     Target_en_us "Restricted"
     Comment "Map label of a gated area (police yard, hospital back, the prison)."
    }
    CustomStringTableItem "{66EFF29951500B8B}" {
     Id "LF-Map_AreaLawless"
     Target_en_us "No {police}"
     Comment "Map label of a lawless area, stamped in red."
    }
    CustomStringTableItem "{5DCCFFA00AEF5C7D}" {
     Id "LF-Map_AreaSafe"
     Target_en_us "Safe zone"
     Comment "Map label of a safe zone at the closest zoom."
    }
    CustomStringTableItem "{7C3A91D5E208B4F6}" {
     Id "LF-Market_BelowLot"
     Target_en_us "This buyer takes whole lots of %1. Bring at least one full lot."
     Comment "Toast to the seller (#3622): the stack is smaller than one lot of a good priced per lot, so nothing was sold. %1 = the units in one lot."
    }
    CustomStringTableItem "{5E8B1D70A3C94F26}" {
     Id "LF-Market_BoardNext"
     Target_en_us "Sell your goods to the trader at the price on the board."
     Comment "Sign catalog 'next' text of the trader price board face (never shown: the board's own Read action opens the prices card)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60718}" {
     Id "LF-Market_BoardTitle"
     Target_en_us "Prices today"
     Comment "Title of the trader chalk price board card."
    }
    CustomStringTableItem "{7B3D5A91C2E0F486}" {
     Id "LF-Market_BoardWhat"
     Target_en_us "What the trader pays today for each good, chalked on the board."
     Comment "Sign catalog 'what' text of the trader price board face (never shown: the board's own Read action opens the prices card)."
    }
    CustomStringTableItem "{3B7C9A10E4D25F81}" {
     Id "LF-Market_BuyerFull"
     Target_en_us "They have enough for now. Try again in about %1 min."
     Comment "Toast to the seller: a premium buyer's demand is spent. %1 = minutes until it takes one more unit (#3599). A pack row with the same Id adds where else to sell."
    }
    CustomStringTableItem "{3B7C9A10E4D25F83}" {
     Id "LF-Market_BuyerFull_Title"
     Target_en_us "Enough for now"
     Comment "Title of the buyer-full toast (#3599)."
    }
    CustomStringTableItem "{3B7C9A10E4D25F82}" {
     Id "LF-Market_BuyerNever"
     Target_en_us "They have all they need of that."
     Comment "Toast to the seller: a premium buyer's demand is spent and never refills (#3599)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60719}" {
     Id "LF-Market_NoPrices"
     Target_en_us "No prices today."
     Comment "Price board line when the market is off or lists no goods."
    }
    CustomStringTableItem "{E3A1B2C4D5F6071A}" {
     Id "LF-Market_Price"
     Target_en_us "%1  %2"
     Comment "Price board line: %1 good name, %2 price."
    }
    CustomStringTableItem "{E3A1B2C4D5F6071B}" {
     Id "LF-Market_PriceDown"
     Target_en_us "%1  %2 %3"
     Comment "Price board line for a good below its base: %1 name, %2 price, %3 down arrow."
    }
    CustomStringTableItem "{E3A1B2C4D5F6071C}" {
     Id "LF-Market_Read"
     Target_en_us "Read today's prices"
     Comment "Action on the trader chalk price board."
    }
    CustomStringTableItem "{E3A1B2C4D5F60723}" {
     Id "LF-Market_SaleDown"
     Target_en_us "I'm paying less for %1 today, I've plenty."
     Comment "Toast from the trader after your sale moved a good into a lower price band. %1 = good name. Never a number (#2923)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60725}" {
     Id "LF-Market_SaleTitle"
     Target_en_us "Trader"
     Comment "Title of the trader price remark toast (#2923)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60724}" {
     Id "LF-Market_SaleUp"
     Target_en_us "I'm paying full price for %1 again."
     Comment "Toast from the trader to the next seller once a good recovered a price band. %1 = good name (#2923)."
    }
    CustomStringTableItem "{66656BD872EA3814}" {
     Id "LF-MedicFleet_Take"
     Target_en_us "Take the %1 ambulance"
     Comment "Action on the medic car pool board (#4432). %1 = the title of the lowest medic rank that draws this ambulance (Orderly, Paramedic)."
    }
    CustomStringTableItem "{ADD3EECB7BFA4D40}" {
     Id "LF-MedicFleet_Title"
     Target_en_us "Ambulance pool"
     Comment "Toast title (#4432): the medic car pool board."
    }
    CustomStringTableItem "{DF4F6E621FD608FA}" {
     Id "LF-MedicRank_Doctor"
     Target_en_us "Doctor"
     Comment "Medic rank title, rung 2 (#2708)"
    }
    CustomStringTableItem "{BA039C69E3728E5E}" {
     Id "LF-MedicRank_KitRefused"
     Target_en_us "Your {medic} rank may not draw this item"
     Comment "Refusal when a medic draws kit above their rank (#2708)"
    }
    CustomStringTableItem "{F45008F52BDF7B2C}" {
     Id "LF-MedicRank_Paramedic"
     Target_en_us "Paramedic"
     Comment "Medic rank title, rung 1 (#2708)"
    }
    CustomStringTableItem "{1E8C9F222375C5BB}" {
     Id "LF-MedicRank_Trainee"
     Target_en_us "Trainee {medic}"
     Comment "Medic rank title, rung 0 (#2708)"
    }
    CustomStringTableItem "{3C71B0A2D4E5F612}" {
     Id "LF-Medic_Downed"
     Target_en_us "You are down and bleeding out. Wait for help, or give up (hold X) after %1 seconds."
     Comment "Toast: shown to a player who goes down."
    }
    CustomStringTableItem "{3C71B0A2D4E5F611}" {
     Id "LF-Medic_GiveUpTooEarly"
     Target_en_us "You can give up in %1 seconds."
     Comment "Toast: give up asked too early. %1 is the seconds still to wait."
    }
    CustomStringTableItem "{3C71B0A2D4E5F610}" {
     Id "LF-Medic_Keybind_GiveUp"
     Target_en_us "Give up (hold, while downed)"
     Comment "Keybind: give up and die while downed."
    }
    CustomStringTableItem "{1ED4B50F195B57BB}" {
     Id "LF-Menu_Close"
     Target_en_us "Close"
     Comment "Generic button that closes a menu"
    }
    CustomStringTableItem "{F0FD6E2EE0E11D73}" {
     Id "LF-Mining_NotOre"
     Target_en_us "That is plain rock. Ore rocks are streaked rust red: swing at those."
     Comment "Toast (first-2h review B8): a pickaxe hit a plain rock, not an ore rock."
    }
    CustomStringTableItem "{93B24FC92EC107AF}" {
     Id "LF-Mission_PlaytestTown_Details"
     Target_en_us "This is a playtest build. Your cash and bank top up when they run low, and every new character owns two test cars, one in the garage and one at the impound. Your first spawn opens form R-89; fill it in to start. The first player on a server with no police is added to the police roster, so you can clock in at the station desk. Progress is saved and carries over a restart. Report problems on the Life Framework Discord or GitHub."
     Comment "Detailed scenario rules shown with the scenario."
    }
    CustomStringTableItem "{014EE9C8731D5B15}" {
     Id "LF-Murder_Title"
     Target_en_us "Murder"
     Comment "Toast title for the killer after a murder."
    }
    CustomStringTableItem "{1CF2A86D80F0C6CA}" {
     Id "LF-Murder_Wanted"
     Target_en_us "You killed someone. You are wanted for murder, and witnesses will describe your clothes and vehicle to the {police}."
     Comment "Toast to the killer after a murder. The police get a description, not a name."
    }
    CustomStringTableItem "{D350D31D83DCF9E2}" {
     Id "LF-NewLife_ReturnToast"
     Target_en_us "New Life Rule: you are near where you died. Please keep away."
     Comment "Quiet reminder toast when a wristband wearer returns to their death scene inside the window."
    }
    CustomStringTableItem "{5934A766F9EB76E9}" {
     Id "LF-NewLife_WakeToast"
     Target_en_us "You wear a New Life wristband. Stay away from where you died for %1 minutes."
     Comment "Toast on waking in hospital after a real death. %1 is the minutes."
    }
    CustomStringTableItem "{3D9D0114019FA819}" {
     Id "LF-NewspaperEdition_ArrestLine"
     Target_en_us "%1 was jailed for %3 minutes."
     Comment "Courier arrest line. %1 name, %3 minutes."
    }
    CustomStringTableItem "{A94FDE8D9CF2A01F}" {
     Id "LF-NewspaperEdition_ArrestLineNoName"
     Target_en_us "A resident was jailed for %3 minutes."
     Comment "Courier arrest line with no name. %3 minutes."
    }
    CustomStringTableItem "{894E6ED818297742}" {
     Id "LF-NewspaperEdition_ArrestsHeadline"
     Target_en_us "Arrests this week"
     Comment "Courier section headline."
    }
    CustomStringTableItem "{22BD063C26FD5B70}" {
     Id "LF-NewspaperEdition_CrimeHeadline"
     Target_en_us "Crimes reported"
     Comment "Courier section headline for crimes reported, never with names."
    }
    CustomStringTableItem "{016F88C3A5AC3DB0}" {
     Id "LF-NewspaperEdition_CrimeLine"
     Target_en_us "%2: %3 reported this week."
     Comment "Courier crime line. %2 offence, %3 count. No names."
    }
    CustomStringTableItem "{806AAD6D47CB71A9}" {
     Id "LF-NewspaperEdition_Dateline"
     Target_en_us "Week %1 edition"
     Comment "Courier dateline: the week of the year."
    }
    CustomStringTableItem "{6A025FF8D8D24473}" {
     Id "LF-NewspaperEdition_MarketHeadline"
     Target_en_us "Market prices"
     Comment "Courier section headline."
    }
    CustomStringTableItem "{3CDD917F8D234034}" {
     Id "LF-NewspaperEdition_MarketLine"
     Target_en_us "%2: was %3, now %4."
     Comment "Courier market line. %2 good, %3 old price, %4 new price."
    }
    CustomStringTableItem "{1B319DCEBD1A3319}" {
     Id "LF-NewspaperEdition_OpeningLine"
     Target_en_us "%1 bought a building plot for %3."
     Comment "Courier plot sale line with a name; the core section prints no names (a plot is a home). %1 name, %3 price."
    }
    CustomStringTableItem "{1C6E4500C2E5F6C7}" {
     Id "LF-NewspaperEdition_OpeningLineNoName"
     Target_en_us "A building plot sold for %3."
     Comment "Courier plot sale line, no name. %3 price."
    }
    CustomStringTableItem "{91D7413A2A1494B4}" {
     Id "LF-NewspaperEdition_OpeningsHeadline"
     Target_en_us "Land sales"
     Comment "Courier section headline: building plots sold this week."
    }
    CustomStringTableItem "{FA7303F7E07F54FE}" {
     Id "LF-NewspaperEdition_RewardLine"
     Target_en_us "%1 collected %3 in call rewards."
     Comment "Courier reward line. %1 name, %3 money."
    }
    CustomStringTableItem "{B70BC567343C4313}" {
     Id "LF-NewspaperEdition_RewardLineNoName"
     Target_en_us "An officer collected %3 in call rewards."
     Comment "Courier reward line with no name. %3 money."
    }
    CustomStringTableItem "{874A06AA2DC6D702}" {
     Id "LF-NewspaperEdition_RewardsHeadline"
     Target_en_us "Rewards paid"
     Comment "Courier section headline."
    }
    CustomStringTableItem "{4DD4F07F0C812A8E}" {
     Id "LF-NewspaperEdition_Someone"
     Target_en_us "A resident"
     Comment "Courier: stands in for a name that is not known."
    }
    CustomStringTableItem "{60ED2D502AEE4A5F}" {
     Id "LF-Newspaper_BoardsBody"
     Target_en_us "The notice boards carry the latest notices, job offers and prices. Read them before you head out."
     Comment "Placeholder story text."
    }
    CustomStringTableItem "{A66899FB35600F3C}" {
     Id "LF-Newspaper_BoardsHeadline"
     Target_en_us "Notices on the boards"
     Comment "Placeholder story headline: pointer to the town boards."
    }
    CustomStringTableItem "{A8404CF269808C13}" {
     Id "LF-Newspaper_Dateline"
     Target_en_us "This week's edition"
     Comment "Newspaper dateline under the masthead, until the edition builder prints one."
    }
    CustomStringTableItem "{969E4E2B63561263}" {
     Id "LF-Newspaper_ItemDesc"
     Target_en_us "This week's edition, folded to a quarter. Read it for the news."
     Comment "Item description: the weekly newspaper (WO-W2.1)."
    }
    CustomStringTableItem "{46E2290C5213FC5C}" {
     Id "LF-Newspaper_ItemName"
     Target_en_us "Newspaper"
     Comment "Item name: the weekly newspaper (WO-W2.1)."
    }
    CustomStringTableItem "{4E0E41414B63DFEF}" {
     Id "LF-Newspaper_Masthead"
     Target_en_us "The Courier"
     Comment "Newspaper masthead. Neutral core text; a pack overrides it with the island name."
    }
    CustomStringTableItem "{BBC844EEF191940C}" {
     Id "LF-Newspaper_QuietBody"
     Target_en_us "Nothing worth printing has happened yet. The next edition will tell you what people did this week."
     Comment "Placeholder story text."
    }
    CustomStringTableItem "{236EB90A4E23A5EB}" {
     Id "LF-Newspaper_QuietHeadline"
     Target_en_us "A quiet week"
     Comment "Placeholder story headline printed before the first edition exists."
    }
    CustomStringTableItem "{0CF5983D9127E9D1}" {
     Id "LF-Newspaper_Read"
     Target_en_us "Read the newspaper"
     Comment "Item action: open the newspaper page (WO-W2.1)."
    }
    CustomStringTableItem "{CA9A4B5596B7F647}" {
     Id "LF-Notify_TitleEms"
     Target_en_us "Emergency"
     Comment "Fallback toast title for an EMS alert with no title of its own."
    }
    CustomStringTableItem "{0EDECFCE6CD087EF}" {
     Id "LF-Notify_TitlePolice"
     Target_en_us "{Police} alert"
     Comment "Fallback toast title for a police alert with no title of its own."
    }
    CustomStringTableItem "{47BF95CEC2D7EE59}" {
     Id "LF-Notify_TitleRobbery"
     Target_en_us "Robbery alarm"
     Comment "Fallback toast title for a robbery alarm with no title of its own."
    }
    CustomStringTableItem "{134C8AE505DFAE31}" {
     Id "LF-Notify_TitleSms"
     Target_en_us "New message"
     Comment "Fallback toast title for a received text message with no title of its own."
    }
    CustomStringTableItem "{B687CB4145329863}" {
     Id "LF-NpcRole_farmer_Bye"
     Target_en_us "Good luck."
     Comment "NPC role talk (farmer), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{4B47713A4CD9AE59}" {
     Id "LF-NpcRole_farmer_Greet"
     Target_en_us "Afternoon. Mind the beds, I just planted."
     Comment "NPC role talk (farmer), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{60C4E4355479468B}" {
     Id "LF-NpcRole_farmer_Name"
     Target_en_us "Farmer"
     Comment "NPC role talk (farmer), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{6D8C2DC519E0E77B}" {
     Id "LF-NpcRole_farmer_Rumour"
     Target_en_us "Somebody has been pulling cabbages at night. I have started to sleep with the window open."
     Comment "NPC role talk (farmer), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{D56BCCA730C6896B}" {
     Id "LF-NpcRole_farmer_What"
     Target_en_us "Cabbages, mostly. Plant them, water them, pull them. They sell at the market square when they are ready."
     Comment "NPC role talk (farmer), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{1430364B918FE31C}" {
     Id "LF-NpcRole_farmer_Where"
     Target_en_us "The market square is down the main street. The traders there pay fair for a good cabbage."
     Comment "NPC role talk (farmer), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{7DDE7D6CEDCBC982}" {
     Id "LF-NpcRole_fence_Name"
     Target_en_us "Fence"
     Comment "NPC role talk (fence), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{3AEC9098FBC89EDC}" {
     Id "LF-NpcRole_logger_Bye"
     Target_en_us "Watch the branches."
     Comment "NPC role talk (logger), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{A933FD945540B9CA}" {
     Id "LF-NpcRole_logger_Greet"
     Target_en_us "Stand back from the trees, friend."
     Comment "NPC role talk (logger), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{B3FE08971BF2884E}" {
     Id "LF-NpcRole_logger_Name"
     Target_en_us "Lumberjack"
     Comment "NPC role talk (logger), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{58613C194CF6CFD9}" {
     Id "LF-NpcRole_logger_Rumour"
     Target_en_us "Somebody left a cut pile at the roadside and nobody has claimed it. I did not take it, whatever they say."
     Comment "NPC role talk (logger), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{6BC073B95CBE6EA6}" {
     Id "LF-NpcRole_logger_What"
     Target_en_us "I fell and cut. The sawmill takes whatever I bring, and firewood sells well before winter."
     Comment "NPC role talk (logger), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{0B503EBDBB0CA03E}" {
     Id "LF-NpcRole_logger_Where"
     Target_en_us "The sawmill is where the road meets the woods. Follow the sound of the saws."
     Comment "NPC role talk (logger), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{53BFD39FAA73A5B0}" {
     Id "LF-NpcRole_mechanic_Name"
     Target_en_us "Mechanic"
     Comment "NPC role talk (mechanic), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{BD15D2974F55B43B}" {
     Id "LF-NpcRole_medic_Name"
     Target_en_us "Doctor"
     Comment "NPC role talk (medic), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{3D5F374C14CE715C}" {
     Id "LF-NpcRole_miner_Bye"
     Target_en_us "Mind the dust."
     Comment "NPC role talk (miner), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{A80544DD70ECF0F2}" {
     Id "LF-NpcRole_miner_Greet"
     Target_en_us "Watch your step, the ground is loose."
     Comment "NPC role talk (miner), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{29B29833F5734928}" {
     Id "LF-NpcRole_miner_Name"
     Target_en_us "Quarry worker"
     Comment "NPC role talk (miner), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{ED66BAC270FEB1FA}" {
     Id "LF-NpcRole_miner_Rumour"
     Target_en_us "They say the old vein is not played out. Somebody has been digging after dark."
     Comment "NPC role talk (miner), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{574CF6DE255947F6}" {
     Id "LF-NpcRole_miner_What"
     Target_en_us "I work the pit. Stone, gravel, sand, cement. Somebody always needs a load, and the pay is steady."
     Comment "NPC role talk (miner), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{17E06A6C1E585D6E}" {
     Id "LF-NpcRole_miner_Where"
     Target_en_us "The builders on the road buy what we dig. Bring a full load, not half."
     Comment "NPC role talk (miner), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{BEB8C35CCD13DF8D}" {
     Id "LF-NpcRole_office_Bye"
     Target_en_us "Good day to you."
     Comment "NPC role talk (office), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{04D367ED1588D453}" {
     Id "LF-NpcRole_office_Greet"
     Target_en_us "Good day. The office is open. What do you need?"
     Comment "NPC role talk (office), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{D2DA65531A3EFDB1}" {
     Id "LF-NpcRole_office_Name"
     Target_en_us "Official"
     Comment "NPC role talk (office), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{8CFD9631DE74BF7E}" {
     Id "LF-NpcRole_office_Rumour"
     Target_en_us "More people arrive every week with no papers. The registry cannot keep up."
     Comment "NPC role talk (office), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{3501F8BF50C8989E}" {
     Id "LF-NpcRole_office_What"
     Target_en_us "I handle the paperwork here. Forms, records, keys and permits. Everything has a stamp, and nothing is quick."
     Comment "NPC role talk (office), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{1ADD957AEEDCC448}" {
     Id "LF-NpcRole_office_Where"
     Target_en_us "The {town} board on the square lists every office and who is hiring. The post office is next to the bank."
     Comment "NPC role talk (office), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{01F9E92E5799351C}" {
     Id "LF-NpcRole_police_Name"
     Target_en_us "{Police} officer"
     Comment "NPC role talk (police), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{168CC753E6805BE1}" {
     Id "LF-NpcRole_ranger_Bye"
     Target_en_us "Walk safe."
     Comment "NPC role talk (ranger), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{8730D7B08E4856F6}" {
     Id "LF-NpcRole_ranger_Greet"
     Target_en_us "Quiet out here. Good. Keep it that way."
     Comment "NPC role talk (ranger), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{7612765CE4B23970}" {
     Id "LF-NpcRole_ranger_Name"
     Target_en_us "Ranger"
     Comment "NPC role talk (ranger), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{F170997FA8278696}" {
     Id "LF-NpcRole_ranger_Rumour"
     Target_en_us "Somebody has been leaving fires burning in the forest. It will not end well."
     Comment "NPC role talk (ranger), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{A1118D74AAFF5068}" {
     Id "LF-NpcRole_ranger_What"
     Target_en_us "I teach people to survive outside {town}. Water, food, a fire, a shelter. The weather turns fast out here."
     Comment "NPC role talk (ranger), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{92AF2B723370C179}" {
     Id "LF-NpcRole_ranger_Where"
     Target_en_us "Walk the road, not the woods, until you know them. The coast is east and the farms are north."
     Comment "NPC role talk (ranger), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{85F76A512682F9A4}" {
     Id "LF-NpcRole_store_clerk_Name"
     Target_en_us "Store clerk"
     Comment "NPC role talk (store_clerk), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{F3F6D2AFCE92D2D0}" {
     Id "LF-NpcRole_teller_Name"
     Target_en_us "Bank teller"
     Comment "NPC role talk (teller), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{0093D099EE56193C}" {
     Id "LF-NpcRole_townsfolk_Name"
     Target_en_us "Resident"
     Comment "NPC role talk (townsfolk), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{0956000C64F43D0E}" {
     Id "LF-NpcRole_trader_Bye"
     Target_en_us "Come back when you have more."
     Comment "NPC role talk (trader), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{43050BA6C01F059E}" {
     Id "LF-NpcRole_trader_Greet"
     Target_en_us "Fresh today. Have a look."
     Comment "NPC role talk (trader), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{AC2EACA2486B5505}" {
     Id "LF-NpcRole_trader_Name"
     Target_en_us "Market trader"
     Comment "NPC role talk (trader), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{EBF9DA4890A4D8CD}" {
     Id "LF-NpcRole_trader_Rumour"
     Target_en_us "The price of apples went up this week. Somebody is hoarding them, I think."
     Comment "NPC role talk (trader), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{5A90080D4C8E4513}" {
     Id "LF-NpcRole_trader_What"
     Target_en_us "I buy and sell at the stall. Bring me what you grow or make and I will give you a fair price."
     Comment "NPC role talk (trader), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{655793D24F3EAED3}" {
     Id "LF-NpcRole_trader_Where"
     Target_en_us "The farm is out past the greenhouse. For anything else, the {town} board on the square will tell you."
     Comment "NPC role talk (trader), from tools/assets/outfits/roles.json."
    }
    CustomStringTableItem "{9FEC139F83A79626}" {
     Id "LF-NpcTalk_Action"
     Target_en_us "Talk"
     Comment "F-key action on an NPC that talks (tools/assets/outfits/roles.json)."
    }
    CustomStringTableItem "{AA90EE0838BF9F1B}" {
     Id "LF-NpcTalk_AskRumour"
     Target_en_us "Heard anything lately?"
     Comment "NPC role talk: reply asking for a rumour."
    }
    CustomStringTableItem "{ED882293148BD52A}" {
     Id "LF-NpcTalk_AskWhat"
     Target_en_us "What do you do here?"
     Comment "NPC role talk: reply asking what the NPC does."
    }
    CustomStringTableItem "{CC94A46527A7AC23}" {
     Id "LF-NpcTalk_AskWhere"
     Target_en_us "Can you point me somewhere?"
     Comment "NPC role talk: reply asking for directions."
    }
    CustomStringTableItem "{2458F9FE031B5D45}" {
     Id "LF-NpcTalk_Bye"
     Target_en_us "Goodbye."
     Comment "NPC role talk: goodbye reply."
    }
    CustomStringTableItem "{8B3B9E5F68DEE66C}" {
     Id "LF-NpcTalk_More"
     Target_en_us "One more thing."
     Comment "NPC role talk: reply to ask something else."
    }
    CustomStringTableItem "{3296F0CB09E2A3D7}" {
     Id "LF-Offence_ArmoryHeist"
     Target_en_us "Police armory heist"
     Comment "Offence: the armory store was breached (#275)."
    }
    CustomStringTableItem "{A1B41FC05FD138B7}" {
     Id "LF-Offence_BreakIn"
     Target_en_us "Breaking and entering"
     Comment "Offence on a wanted poster: a door was breached."
    }
    CustomStringTableItem "{18003FE63D1892AD}" {
     Id "LF-Offence_Carjacking"
     Target_en_us "Vehicle theft"
     Comment "Offence on a wanted poster: a vehicle was taken from its driver by force (crime catalog id; same words as LF-Offence_VehicleTheft today)."
    }
    CustomStringTableItem "{C3E5A18D0842729C}" {
     Id "LF-Offence_CashVan"
     Target_en_us "Cash van robbery"
     Comment "Offence: the cash van box was forced (#274)."
    }
    CustomStringTableItem "{5EA2C4A000000001}" {
     Id "LF-Offence_ChainGoods"
     Target_en_us "Carrying contraband goods"
     Comment "Offence: chain goods found on an officer search (#419)."
    }
    CustomStringTableItem "{BD444F2178AE712F}" {
     Id "LF-Offence_ChopShop"
     Target_en_us "Chop shop dismantling"
     Comment "Offence on a wanted poster: a stolen car was chopped (#415)."
    }
    CustomStringTableItem "{6860E84495F88846}" {
     Id "LF-Offence_DrunkDriving"
     Target_en_us "Drunk driving"
     Comment "Offence a licence seizure rule names (#3428): driving while impaired. Not in the crime catalog yet."
    }
    CustomStringTableItem "{3B7D5E1A94C2F608}" {
     Id "LF-Offence_EscapedCustody"
     Target_en_us "Escaped custody"
     Comment "Offence on a wanted poster: a prisoner walked out of the jail in a break window (#3422)."
    }
    CustomStringTableItem "{6E4CC71CE2AF3D51}" {
     Id "LF-Offence_HospitalRobbery"
     Target_en_us "Hospital robbery"
     Comment "Offence on a wanted poster."
    }
    CustomStringTableItem "{9499603B0EEA25D2}" {
     Id "LF-Offence_IllegalItem"
     Target_en_us "Carrying contraband"
     Comment "Offence: carrying an item that is illegal for anyone (#988)."
    }
    CustomStringTableItem "{C4A100000000B101}" {
     Id "LF-Offence_JailBreak"
     Target_en_us "Jailbreak"
     Comment "Offence: a prisoner walked out through a breached jail door (#235)."
    }
    CustomStringTableItem "{E62CDBBA4E99A489}" {
     Id "LF-Offence_Lockpick"
     Target_en_us "Lock picking"
     Comment "Offence on a wanted poster: a lock was picked (#411)."
    }
    CustomStringTableItem "{0A1E0465064F3E5F}" {
     Id "LF-Offence_Murder"
     Target_en_us "Murder"
     Comment "Offence on a wanted poster: the suspect killed a person (M1-27)."
    }
    CustomStringTableItem "{E95567BA027671E4}" {
     Id "LF-Offence_PrisonBreach"
     Target_en_us "Breaking prisoners out"
     Comment "Offence: blew the jail cells open to free the prisoners (two-layer jailbreak)."
    }
    CustomStringTableItem "{467D009B1D72E627}" {
     Id "LF-Offence_RestrictedItem"
     Target_en_us "Carrying restricted equipment"
     Comment "Offence: carrying equipment only a licence holder may carry, such as police kit (#988)."
    }
    CustomStringTableItem "{75DBEF394E6BF363}" {
     Id "LF-Offence_Robbery"
     Target_en_us "Armed robbery"
     Comment "Offence on a wanted poster: a till or safe was robbed."
    }
    CustomStringTableItem "{3696651FF2025033}" {
     Id "LF-Offence_Scanner"
     Target_en_us "Carrying a police scanner"
     Comment "Offence: a scanner found on a search (#253)."
    }
    CustomStringTableItem "{C7C8981D12E7794E}" {
     Id "LF-Offence_Shoplifting"
     Target_en_us "Shoplifting"
     Comment "Offence on a wanted poster: left a store with unpaid goods (#202)."
    }
    CustomStringTableItem "{02730936057A3FB7}" {
     Id "LF-Offence_UnlicensedItem"
     Target_en_us "Carrying without a license"
     Comment "Offence: carrying an item that needs a licence (a handgun without a firearm permit) (#988)."
    }
    CustomStringTableItem "{1FEDD190A93DC8BE}" {
     Id "LF-Offence_UnpaidFine"
     Target_en_us "Unpaid fine"
     Comment "Wanted board offence when a police ticket goes unpaid past its second deadline."
    }
    CustomStringTableItem "{9B5C5EBC51647BBD}" {
     Id "LF-Offence_VehicleTheft"
     Target_en_us "Vehicle theft"
     Comment "Offence on a wanted poster."
    }
    CustomStringTableItem "{E2A9C867DDFB5021}" {
     Id "LF-Offence_WeaponTheft"
     Target_en_us "Weapon theft"
     Comment "Offence on a wanted poster."
    }
    CustomStringTableItem "{28833077A8307105}" {
     Id "LF-Ops_RestartTitle"
     Target_en_us "Server restart"
     Comment "Toast title for the scheduled restart warning (Ops O6, #1265)."
    }
    CustomStringTableItem "{40655A61760C8E55}" {
     Id "LF-Ops_RestartToast"
     Target_en_us "The server restarts in %1 minute(s)."
     Comment "Toast to every player at each restart warning step (Ops O6, #1265). %1 = minutes left."
    }
    CustomStringTableItem "{5D17A30C9E4B8F21}" {
     Id "LF-OreGate_Title"
     Target_en_us "Too hard for you yet"
     Comment "Toast title when a pickaxe swing at an ore rock is refused for a low job level (a gated node row, #3243); the body is the job-level refusal in words."
    }
    CustomStringTableItem "{7D34417334CA2E12}" {
     Id "LF-OrgFlagWall_Done"
     Target_en_us "Your gang now flies the new flag."
     Comment "Toast to the gang leader after picking a flag at the wall."
    }
    CustomStringTableItem "{0DD2469217004277}" {
     Id "LF-OrgFlagWall_Fly"
     Target_en_us "Fly the %1 flag"
     Comment "Action on a sample flag at the registry flag wall. %1 is the flag name."
    }
    CustomStringTableItem "{5835D1DD383908D7}" {
     Id "LF-OrgFlagWall_Held"
     Target_en_us "That flag belongs to %1, and they are around. Pick another."
     Comment "Toast when a flag is held by a gang with a member online. %1 is the holding gang."
    }
    CustomStringTableItem "{B717A91F51503F78}" {
     Id "LF-OrgFlagWall_NoGang"
     Target_en_us "You need a gang to fly a flag."
     Comment "Toast to a player with no organization."
    }
    CustomStringTableItem "{5592F38460C60DB0}" {
     Id "LF-OrgFlagWall_NotLeader"
     Target_en_us "Only a gang leader can pick the gang flag."
     Comment "Toast to a gang member who is not a leader."
    }
    CustomStringTableItem "{BDE4CC259DADC42D}" {
     Id "LF-OrgFlagWall_Refused"
     Target_en_us "That flag cannot be picked."
     Comment "Toast when the pick is not one of the preset flags."
    }
    CustomStringTableItem "{2317577E1FF2C9F2}" {
     Id "LF-OrgFlagWall_Same"
     Target_en_us "Your gang already flies that flag."
     Comment "Toast when the leader picks the flag the gang already has."
    }
    CustomStringTableItem "{6DD4905328743C3C}" {
     Id "LF-OrgFlag_Name_01"
     Target_en_us "Flag 1"
     Comment "Neutral name of preset gang flag 1; a pack gives the real name."
    }
    CustomStringTableItem "{C7E4384645F27400}" {
     Id "LF-OrgFlag_Name_02"
     Target_en_us "Flag 2"
     Comment "Neutral name of preset gang flag 2; a pack gives the real name."
    }
    CustomStringTableItem "{06CB274D16F5D514}" {
     Id "LF-OrgFlag_Name_03"
     Target_en_us "Flag 3"
     Comment "Neutral name of preset gang flag 3; a pack gives the real name."
    }
    CustomStringTableItem "{4BBB49F3186E9C91}" {
     Id "LF-OrgFlag_Name_04"
     Target_en_us "Flag 4"
     Comment "Neutral name of preset gang flag 4; a pack gives the real name."
    }
    CustomStringTableItem "{80848B99F9327F1F}" {
     Id "LF-OrgFlag_Name_05"
     Target_en_us "Flag 5"
     Comment "Neutral name of preset gang flag 5; a pack gives the real name."
    }
    CustomStringTableItem "{41BB2518650EC259}" {
     Id "LF-OrgFlag_Name_06"
     Target_en_us "Flag 6"
     Comment "Neutral name of preset gang flag 6; a pack gives the real name."
    }
    CustomStringTableItem "{3BFCE726AB706918}" {
     Id "LF-OrgFlag_Name_07"
     Target_en_us "Flag 7"
     Comment "Neutral name of preset gang flag 7; a pack gives the real name."
    }
    CustomStringTableItem "{D549594C3E7CA88D}" {
     Id "LF-OrgFlag_Name_08"
     Target_en_us "Flag 8"
     Comment "Neutral name of preset gang flag 8; a pack gives the real name."
    }
    CustomStringTableItem "{CD1F50FA7D06680B}" {
     Id "LF-OrgFlag_Name_09"
     Target_en_us "Flag 9"
     Comment "Neutral name of preset gang flag 9; a pack gives the real name."
    }
    CustomStringTableItem "{F2D8608EB4BADFA2}" {
     Id "LF-OrgFlag_Name_10"
     Target_en_us "Flag 10"
     Comment "Neutral name of preset gang flag 10; a pack gives the real name."
    }
    CustomStringTableItem "{1F4EC0E2CC8A7E46}" {
     Id "LF-OrgFlag_Name_11"
     Target_en_us "Flag 11"
     Comment "Neutral name of preset gang flag 11; a pack gives the real name."
    }
    CustomStringTableItem "{9896D7B92896EC0A}" {
     Id "LF-OrgFlag_Name_12"
     Target_en_us "Flag 12"
     Comment "Neutral name of preset gang flag 12; a pack gives the real name."
    }
    CustomStringTableItem "{82527DEB453A9885}" {
     Id "LF-OrgFlag_Name_13"
     Target_en_us "Flag 13"
     Comment "Neutral name of preset gang flag 13; a pack gives the real name."
    }
    CustomStringTableItem "{914669FAB5989DD9}" {
     Id "LF-OrgFlag_Name_14"
     Target_en_us "Flag 14"
     Comment "Neutral name of preset gang flag 14; a pack gives the real name."
    }
    CustomStringTableItem "{71BE884AF0AC76CD}" {
     Id "LF-OrgFlag_Name_15"
     Target_en_us "Flag 15"
     Comment "Neutral name of preset gang flag 15; a pack gives the real name."
    }
    CustomStringTableItem "{1625AAF66BB314BA}" {
     Id "LF-OrgFlag_Name_16"
     Target_en_us "Flag 16"
     Comment "Neutral name of preset gang flag 16; a pack gives the real name."
    }
    CustomStringTableItem "{1CB2F49578C1F055}" {
     Id "LF-OrgFlag_Name_17"
     Target_en_us "Flag 17"
     Comment "Neutral name of preset gang flag 17; a pack gives the real name."
    }
    CustomStringTableItem "{57C899AC492EF9AC}" {
     Id "LF-OrgFlag_Name_18"
     Target_en_us "Flag 18"
     Comment "Neutral name of preset gang flag 18; a pack gives the real name."
    }
    CustomStringTableItem "{0A2B3522F3279827}" {
     Id "LF-OrgFlag_Name_19"
     Target_en_us "Flag 19"
     Comment "Neutral name of preset gang flag 19; a pack gives the real name."
    }
    CustomStringTableItem "{2323BD897B6C077E}" {
     Id "LF-OrgFlag_Name_20"
     Target_en_us "Flag 20"
     Comment "Neutral name of preset gang flag 20; a pack gives the real name."
    }
    CustomStringTableItem "{D1325EB69512F8BF}" {
     Id "LF-OrgProspect_Rank"
     Target_en_us "Prospect"
     Comment "Rank shown for a gang member who joined by invite less than 48 hours ago (#4455). No countdown."
    }
    CustomStringTableItem "{A033D8712C95A9F9}" {
     Id "LF-OrgProspect_Refused"
     Target_en_us "Prospects can't touch the gang fund or the stash yet."
     Comment "Refusal for a gang prospect at the fund or the stash (#4455)."
    }
    CustomStringTableItem "{5AB8DC50BD7DC910}" {
     Id "LF-OrgRight_BUY_UPGRADE"
     Target_en_us "Buy upgrades"
     Comment "Organisation right name, as a gated outlet's refusal names it (#988)."
    }
    CustomStringTableItem "{6CA987E909812994}" {
     Id "LF-OrgRight_DECLARE_WAR"
     Target_en_us "Declare war"
     Comment "Organisation right name, as a gated outlet's refusal names it (#988)."
    }
    CustomStringTableItem "{83B81936948E8F2E}" {
     Id "LF-OrgRight_INVITE"
     Target_en_us "Invite members"
     Comment "Organisation right name, as a gated outlet's refusal names it (#988)."
    }
    CustomStringTableItem "{665DB356B526C676}" {
     Id "LF-OrgRight_KICK"
     Target_en_us "Remove members"
     Comment "Organisation right name, as a gated outlet's refusal names it (#988)."
    }
    CustomStringTableItem "{DFC65A7B39D19F53}" {
     Id "LF-OrgRight_MANAGE_RANKS"
     Target_en_us "Manage ranks"
     Comment "Organisation right name, as a gated outlet's refusal names it (#988)."
    }
    CustomStringTableItem "{69DB6A2760E0386A}" {
     Id "LF-OrgRight_MANAGE_TERRITORY"
     Target_en_us "Manage territory"
     Comment "Organisation right name, as a gated outlet's refusal names it (#988)."
    }
    CustomStringTableItem "{C99C38C351DB03E8}" {
     Id "LF-OrgRight_SPEND_FUND"
     Target_en_us "Spend the fund"
     Comment "Organisation right name, as a gated outlet's refusal names it (#988)."
    }
    CustomStringTableItem "{35B5C1078661D599}" {
     Id "LF-OrgRight_USE_STASH"
     Target_en_us "Use the stash"
     Comment "Organisation right name, as a gated outlet's refusal names it (#988)."
    }
    CustomStringTableItem "{79098ED875FE1D2B}" {
     Id "LF-OrgRight_WITHDRAW"
     Target_en_us "Withdraw from the fund"
     Comment "Organisation right name, as a gated outlet's refusal names it (#988)."
    }
    CustomStringTableItem "{C65C0D31073BC4C5}" {
     Id "LF-OrgWarAnnounce_Accepted"
     Target_en_us "War begins: %1"
     Comment "Island toast: the target answered, the two organizations are at war; %1 is 'Declarer / Target' (#2669)."
    }
    CustomStringTableItem "{E7E4127834BBA4FA}" {
     Id "LF-OrgWarAnnounce_Declared"
     Target_en_us "War declared: %1"
     Comment "Island toast: one organization declared war on another; %1 is 'Declarer / Target' (#2669)."
    }
    CustomStringTableItem "{7B94EDBB4BFE6C05}" {
     Id "LF-OrgWarAnnounce_Forfeit"
     Target_en_us "War forfeited, an organization has no members left: %1"
     Comment "Island toast: an organization emptied and forfeited its war; %1 is 'Declarer / Target' (#2669)."
    }
    CustomStringTableItem "{BED6AA1BA673F932}" {
     Id "LF-OrgWarAnnounce_Lapsed"
     Target_en_us "Declaration lapsed, nobody answered: %1"
     Comment "Island toast: a declaration went unanswered; %1 is 'Declarer / Target' (#2669)."
    }
    CustomStringTableItem "{7ABE18BB5495091F}" {
     Id "LF-OrgWarAnnounce_MemberAccepted"
     Target_en_us "Your organization is at war with %1."
     Comment "Member notice: the war started; %1 is the other organization (#2669)."
    }
    CustomStringTableItem "{D7EF843062AAD3B6}" {
     Id "LF-OrgWarAnnounce_MemberDeclaredIn"
     Target_en_us "%1 declared war on your organization. Answer at the war desk before the window closes."
     Comment "Member notice: your side was challenged; %1 is the declaring organization (#2669)."
    }
    CustomStringTableItem "{32DA70C1A2DADF42}" {
     Id "LF-OrgWarAnnounce_MemberDeclaredOut"
     Target_en_us "Your organization declared war on %1. They have a while to answer."
     Comment "Member notice: your side declared; %1 is the other organization (#2669)."
    }
    CustomStringTableItem "{3AD6CA5542B4E1BC}" {
     Id "LF-OrgWarAnnounce_MemberForfeit"
     Target_en_us "Your war with %1 ended: an organization has no members left."
     Comment "Member notice: forfeit; %1 is the other organization (#2669)."
    }
    CustomStringTableItem "{EFC7E0BD1866B610}" {
     Id "LF-OrgWarAnnounce_MemberLapsed"
     Target_en_us "The war declaration between your organization and %1 lapsed."
     Comment "Member notice: the declaration went unanswered; %1 is the other organization (#2669)."
    }
    CustomStringTableItem "{B4C1B2C3C2F5929B}" {
     Id "LF-OrgWarAnnounce_MemberTime"
     Target_en_us "Your war with %1 is over, time is up."
     Comment "Member notice: the war ran out; %1 is the other organization (#2669)."
    }
    CustomStringTableItem "{C90E4128872ED67A}" {
     Id "LF-OrgWarAnnounce_Time"
     Target_en_us "War over, time is up: %1"
     Comment "Island toast: a war ran out; %1 is 'Declarer / Target' (#2669)."
    }
    CustomStringTableItem "{B604BC0B97BFA7A1}" {
     Id "LF-OrgWarAnnounce_Title"
     Target_en_us "Gang war"
     Comment "Toast title for organization war announcements (#2669)."
    }
    CustomStringTableItem "{1CC3B7AA55E0C10E}" {
     Id "LF-OrgWarBoard_Accepted"
     Target_en_us "%1 and %2 are at war. The war ends in %3."
     Comment "War notice: a war in progress. %1 and %2 the sides, %3 time left (#2677)."
    }
    CustomStringTableItem "{62AF3CB71CC1FDA0}" {
     Id "LF-OrgWarBoard_Declared"
     Target_en_us "%1 has declared war on %2. An answer is due in %3."
     Comment "War notice: a declaration waiting for the answer. %1 declarer, %2 target, %3 time left (#2677)."
    }
    CustomStringTableItem "{C0F0A8AD758D2916}" {
     Id "LF-OrgWarBoard_Ended"
     Target_en_us "The war between %1 and %2 is over. It ended %3 ago."
     Comment "War notice: a war that ran out. %3 = time since the end (#2677)."
    }
    CustomStringTableItem "{E119F9C79945E8A5}" {
     Id "LF-OrgWarBoard_Forfeit"
     Target_en_us "%1 and %2: the war ended when a side gave up, %3 ago."
     Comment "War notice: a war ended by forfeit. %3 = time since the end (#2677)."
    }
    CustomStringTableItem "{718F296CDB0C307A}" {
     Id "LF-OrgWarBoard_HoursMinutes"
     Target_en_us "%1 h %2 min"
     Comment "War notice time span from an hour up. %1 = hours, %2 = minutes (#2677)."
    }
    CustomStringTableItem "{B6581C263E795E36}" {
     Id "LF-OrgWarBoard_Lapsed"
     Target_en_us "%1 declared war on %2, but nobody answered. It lapsed %3 ago."
     Comment "War notice: a declaration nobody answered. %3 = time since it lapsed (#2677)."
    }
    CustomStringTableItem "{7714D5DEF74ACFA3}" {
     Id "LF-OrgWarBoard_Minutes"
     Target_en_us "%1 min"
     Comment "War notice time span under an hour. %1 = minutes (#2677)."
    }
    CustomStringTableItem "{367833D38BB24DC8}" {
     Id "LF-OrgWarBoard_None"
     Target_en_us "No war has been declared. It is quiet."
     Comment "War notice board line when no notice is up (#2677)."
    }
    CustomStringTableItem "{15794F43B5EA9503}" {
     Id "LF-OrgWarBoard_Read"
     Target_en_us "Read the war notices"
     Comment "Action on the war notice board (#2677)."
    }
    CustomStringTableItem "{57A988E869A2BC64}" {
     Id "LF-OrgWarBoard_SiteFree"
     Target_en_us "%1: unclaimed"
     Comment "War board line for a contested site nobody holds. %1 = the site (#2834)."
    }
    CustomStringTableItem "{FE1828BDD0D5E92B}" {
     Id "LF-OrgWarBoard_SiteHeld"
     Target_en_us "%1: held by %2"
     Comment "War board line for a contested site. %1 = the site, %2 = the holding organisation (#2834)."
    }
    CustomStringTableItem "{37F262A15C951C6C}" {
     Id "LF-OrgWarBoard_SitesHeader"
     Target_en_us "Contested ground"
     Comment "War board line above the contested sites and their holders (#2834)."
    }
    CustomStringTableItem "{4FA0A019D97109D5}" {
     Id "LF-OrgWarBoard_Title"
     Target_en_us "War notices"
     Comment "Title of the war notice board card (#2677)."
    }
    CustomStringTableItem "{E2D1E8610799E24F}" {
     Id "LF-OrgWarSheet_Accept"
     Target_en_us "Accept"
     Comment "War sheet button: accept the declaration in the row (#2675)."
    }
    CustomStringTableItem "{66B687D491E42733}" {
     Id "LF-OrgWarSheet_ActiveLine"
     Target_en_us "Wars in progress (%1)"
     Comment "War sheet line above the active wars. %1 = count (#2675)."
    }
    CustomStringTableItem "{7A53D42CF1DC03B7}" {
     Id "LF-OrgWarSheet_AnswerBy"
     Target_en_us "Answer within %1"
     Comment "War sheet pending row status. %1 = time left to answer, such as 9 min (#2675)."
    }
    CustomStringTableItem "{FAF591F49E6D3C03}" {
     Id "LF-OrgWarSheet_Close"
     Target_en_us "Close"
     Comment "War sheet close button (#2675)."
    }
    CustomStringTableItem "{0CC38A7BD8CFF0E5}" {
     Id "LF-OrgWarSheet_Declare"
     Target_en_us "Declare"
     Comment "War sheet button: declare war on the organisation in the row (#2675)."
    }
    CustomStringTableItem "{0985BBB12E38FF3B}" {
     Id "LF-OrgWarSheet_EndsIn"
     Target_en_us "Ends in %1"
     Comment "War sheet active row status. %1 = time until the war ends (#2675)."
    }
    CustomStringTableItem "{EC6D5BB1509DD4D0}" {
     Id "LF-OrgWarSheet_Intro"
     Target_en_us "Declaring a war costs %1 from your fund. Both sides need at least %2 members. The other side has a short time to answer."
     Comment "War sheet intro line. %1 = the fee as money, %2 = the member minimum (#2675)."
    }
    CustomStringTableItem "{1954F9D1B9794DC7}" {
     Id "LF-OrgWarSheet_IntroNoOrg"
     Target_en_us "You belong to no {org}, so you cannot declare or answer a war. The register is below. A declaration costs %1 and both sides need %2 members."
     Comment "War sheet intro for a viewer with no organisation. %1 = fee, %2 = member minimum (#2675)."
    }
    CustomStringTableItem "{A554181130589FE1}" {
     Id "LF-OrgWarSheet_IntroNoRight"
     Target_en_us "Your rank cannot declare or answer a war. A declaration costs %1 and both sides need %2 members."
     Comment "War sheet intro for a member whose rank lacks the war right. %1 = fee, %2 = member minimum (#2675)."
    }
    CustomStringTableItem "{CF5337F9DE14C7CD}" {
     Id "LF-OrgWarSheet_IntroTooFew"
     Target_en_us "Your {org} needs %2 members to declare or answer a war. A declaration costs %1."
     Comment "War sheet intro when the viewer's organisation is under the member minimum. %1 = fee, %2 = minimum (#2675)."
    }
    CustomStringTableItem "{E3E3577C0775B16B}" {
     Id "LF-OrgWarSheet_Members"
     Target_en_us "%1 members"
     Comment "War sheet register row status. %1 = member count (#2675)."
    }
    CustomStringTableItem "{1D6443E6EB523B34}" {
     Id "LF-OrgWarSheet_Open"
     Target_en_us "Wars"
     Comment "Registry desk action name: opens the war sheet (#2675)."
    }
    CustomStringTableItem "{6E46E29A07149C56}" {
     Id "LF-OrgWarSheet_OrgsLine"
     Target_en_us "On the register (%1)"
     Comment "War sheet line above the register list. %1 = count (#2675)."
    }
    CustomStringTableItem "{A728D68B4D022A0F}" {
     Id "LF-OrgWarSheet_PendingLine"
     Target_en_us "Declared against you (%1)"
     Comment "War sheet line above the declarations made on the viewer's organisation. %1 = count (#2675)."
    }
    CustomStringTableItem "{0C1332EF71C3032A}" {
     Id "LF-OrgWarSheet_StateAtWar"
     Target_en_us "At war"
     Comment "War sheet register row status: the two are at war (#2675)."
    }
    CustomStringTableItem "{596E0A35FB7B60FB}" {
     Id "LF-OrgWarSheet_StateCooldown"
     Target_en_us "Too soon to declare"
     Comment "War sheet register row status: the pair cooldown after a war is running (#2675)."
    }
    CustomStringTableItem "{E81FDC27EE4A5512}" {
     Id "LF-OrgWarSheet_StatePending"
     Target_en_us "Declaration waiting"
     Comment "War sheet register row status: a declaration between the two is waiting for an answer (#2675)."
    }
    CustomStringTableItem "{586D525A4BF713EF}" {
     Id "LF-OrgWarSheet_StateTooFew"
     Target_en_us "Under %1 members"
     Comment "War sheet register row status: the organisation is under the member minimum. %1 = minimum (#2675)."
    }
    CustomStringTableItem "{CB90D6E283C0FEB5}" {
     Id "LF-OrgWarSheet_Title"
     Target_en_us "Organization wars"
     Comment "War sheet title (#2675)."
    }
    CustomStringTableItem "{7E0E46C2E4A95435}" {
     Id "LF-OrgWar_Accepted"
     Target_en_us "Declaration answered. Your organizations are at war."
     Comment "Toast: the target accepted the declaration (#2668)."
    }
    CustomStringTableItem "{1BC0FE9D5A782876}" {
     Id "LF-OrgWar_Declared"
     Target_en_us "War declared. The other organization has until the window closes to answer."
     Comment "Toast: the declaration went through and the fee left the fund (#2668)."
    }
    CustomStringTableItem "{325F9ECB97909DF2}" {
     Id "LF-OrgWar_RefuseCooldown"
     Target_en_us "These organizations fought too recently to declare again."
     Comment "Toast: declaration refused, the pair cooldown is running (#2668)."
    }
    CustomStringTableItem "{211295C1DAB28A4F}" {
     Id "LF-OrgWar_RefuseExpired"
     Target_en_us "The answer window has closed; the declaration lapsed."
     Comment "Toast: answer refused, the declaration lapsed (#2668)."
    }
    CustomStringTableItem "{192720B36BC76B20}" {
     Id "LF-OrgWar_RefuseFund"
     Target_en_us "The organization fund cannot cover the war fee."
     Comment "Toast: declaration refused, fund short of the fee (#2668)."
    }
    CustomStringTableItem "{56FB87313A015E3A}" {
     Id "LF-OrgWar_RefuseGone"
     Target_en_us "That organization or declaration no longer exists."
     Comment "Toast: war request refused, the org or war is gone or already answered (#2668)."
    }
    CustomStringTableItem "{CD36D2312320E8CE}" {
     Id "LF-OrgWar_RefuseNoOrg"
     Target_en_us "You are not in an organization."
     Comment "Toast: war request refused, the player has no organization (#2668)."
    }
    CustomStringTableItem "{4349F738359D727D}" {
     Id "LF-OrgWar_RefuseNoRight"
     Target_en_us "Your rank cannot declare or answer a war."
     Comment "Toast: war request refused, the rank lacks the war right (#2668)."
    }
    CustomStringTableItem "{0C78381E5C917C43}" {
     Id "LF-OrgWar_RefuseNotTarget"
     Target_en_us "That declaration was not made on your organization."
     Comment "Toast: answer refused, the player is not in the declared-on organization (#2668)."
    }
    CustomStringTableItem "{EA8BDFF1CAD3449B}" {
     Id "LF-OrgWar_RefuseOtherTooFew"
     Target_en_us "The other organization has too few members for a war."
     Comment "Toast: war request refused, the other organization below the member minimum (#2668)."
    }
    CustomStringTableItem "{D2F018F47366235B}" {
     Id "LF-OrgWar_RefusePending"
     Target_en_us "A declaration or war between these organizations is already running."
     Comment "Toast: declaration refused, the pair already has a pending or active war (#2668)."
    }
    CustomStringTableItem "{84D688375916FAB8}" {
     Id "LF-OrgWar_RefuseSelf"
     Target_en_us "An organization cannot declare war on itself."
     Comment "Toast: declaration refused, target is the own organization (#2668)."
    }
    CustomStringTableItem "{25A42815D66DCE2F}" {
     Id "LF-OrgWar_RefuseTooFew"
     Target_en_us "Your organization has too few members for a war."
     Comment "Toast: war request refused, own organization below the member minimum (#2668)."
    }
    CustomStringTableItem "{AD3C4AAA651C8DCC}" {
     Id "LF-OrgWar_Refused"
     Target_en_us "The war desk refused that."
     Comment "Toast: a war request was refused for a reason with no text of its own (#2668)."
    }
    CustomStringTableItem "{85E483079A7BF56C}" {
     Id "LF-OrgWar_Title"
     Target_en_us "War desk"
     Comment "Toast title for organization war declarations and answers (#2668)."
    }
    CustomStringTableItem "{AF38925B41949B01}" {
     Id "LF-Org_AlreadyMember"
     Target_en_us "You already belong to an {org}."
     Comment "Founding refusal: already a member."
    }
    CustomStringTableItem "{ACCC2C27C1D1B7A8}" {
     Id "LF-Org_Bought"
     Target_en_us "Upgrade bought from the fund."
     Comment "Ledger notice after an upgrade."
    }
    CustomStringTableItem "{F0174EC746BF1252}" {
     Id "LF-Org_BuyFor"
     Target_en_us "Buy (%1)"
     Comment "Ledger row action. %1 is the price."
    }
    CustomStringTableItem "{461C2E397686A4EA}" {
     Id "LF-Org_BuyRefused"
     Target_en_us "The upgrade was refused."
     Comment "Ledger notice: upgrade refused."
    }
    CustomStringTableItem "{896B3C564891420F}" {
     Id "LF-Org_CashRefused"
     Target_en_us "The clerk refused that amount."
     Comment "Ledger notice: deposit or withdrawal refused."
    }
    CustomStringTableItem "{155A971B5EE37AD7}" {
     Id "LF-Org_CloseLedger"
     Target_en_us "Close the ledger"
     Comment "Ledger close button."
    }
    CustomStringTableItem "{D2628F8C9104150A}" {
     Id "LF-Org_Decline"
     Target_en_us "Decline %1"
     Comment "Interaction entry: turn down the offer of the person faced. %1 is the organization."
    }
    CustomStringTableItem "{1686AD303E7B2F40}" {
     Id "LF-Org_DeclineGeneric"
     Target_en_us "Decline"
     Comment "Fallback name of the decline entry; the handler names the organization."
    }
    CustomStringTableItem "{E7F11EA621C1BA74}" {
     Id "LF-Org_Declined"
     Target_en_us "You turned down %1."
     Comment "Toast to the invitee after Decline. %1 is the organization."
    }
    CustomStringTableItem "{BA50E8C027423E0C}" {
     Id "LF-Org_DemoteGeneric"
     Target_en_us "Demote"
     Comment "Interaction entry fallback: move a member one rank down."
    }
    CustomStringTableItem "{CCC520E4A6F14395}" {
     Id "LF-Org_DemoteIn"
     Target_en_us "Demote in %1"
     Comment "Interaction entry: a member with the manage-ranks right moves the member faced one rank down. %1 is the organization."
    }
    CustomStringTableItem "{D2F38ED8270CB008}" {
     Id "LF-Org_Deposited"
     Target_en_us "Deposited into the fund."
     Comment "Ledger notice after a deposit."
    }
    CustomStringTableItem "{C7FD7A0ABE2999FE}" {
     Id "LF-Org_FoundButton"
     Target_en_us "Sign and pay {currency:sym}%1"
     Comment "Ledger founding button. %1 is the fee."
    }
    CustomStringTableItem "{6FEEAF6F48E6E38E}" {
     Id "LF-Org_FoundFailed"
     Target_en_us "The clerk could not register it. Your fee was returned."
     Comment "Founding failed after the fee; fee returned."
    }
    CustomStringTableItem "{D45EFB759CEED8F8}" {
     Id "LF-Org_FoundIntro"
     Target_en_us "Register a new {org} here. Write its name once; the clerk takes a {currency:sym}%1 fee in cash."
     Comment "Ledger founding line. %1 is the fee."
    }
    CustomStringTableItem "{E8FFE477BDEE452B}" {
     Id "LF-Org_Founded"
     Target_en_us "%1 is in the register."
     Comment "Toast after founding. %1 is the organization name."
    }
    CustomStringTableItem "{50EDA5787944C496}" {
     Id "LF-Org_FoundedLine"
     Target_en_us "Signed. The page is yours."
     Comment "Ledger notice line after founding."
    }
    CustomStringTableItem "{15CA4CCA7E9E4914}" {
     Id "LF-Org_GiveUpLease"
     Target_en_us "Give up"
     Comment "Ledger row action: the holder gives the hideout yard lease back (#2572)."
    }
    CustomStringTableItem "{6D53E58AC9FADED3}" {
     Id "LF-Org_Hideout"
     Target_en_us "Yard lease"
     Comment "Ledger row label: the hideout yard off the registry alley."
    }
    CustomStringTableItem "{CF1AB48D810C8C66}" {
     Id "LF-Org_InviteGeneric"
     Target_en_us "Organization"
     Comment "Interaction entry fallback name for the org_invite slot."
    }
    CustomStringTableItem "{E12E744484FC2C12}" {
     Id "LF-Org_InviteHint"
     Target_en_us "To invite someone, face them and choose Invite."
     Comment "Ledger hint for leaders: invites are face to face."
    }
    CustomStringTableItem "{99CE77E7DA096BC0}" {
     Id "LF-Org_InviteTo"
     Target_en_us "Invite to %1"
     Comment "Interaction entry: a leader invites the person faced. %1 is the organization."
    }
    CustomStringTableItem "{20AD09A604FD14B1}" {
     Id "LF-Org_Join"
     Target_en_us "Join %1"
     Comment "Interaction entry: accept the offer of the person faced. %1 is the organization."
    }
    CustomStringTableItem "{571CB642933950D6}" {
     Id "LF-Org_JoinRefused"
     Target_en_us "The offer is gone. Ask again."
     Comment "Toast to the invitee: join refused."
    }
    CustomStringTableItem "{0703409DF8667523}" {
     Id "LF-Org_Joined"
     Target_en_us "You joined %1."
     Comment "Toast to the new member. %1 is the organization."
    }
    CustomStringTableItem "{624F7970B1B5A487}" {
     Id "LF-Org_LeaseFree"
     Target_en_us "Free"
     Comment "Hideout lease status: nobody holds it."
    }
    CustomStringTableItem "{FAF2A876ADF64AE5}" {
     Id "LF-Org_LeaseGivenUp"
     Target_en_us "You gave the yard back. Its gate is shut."
     Comment "Ledger notice after the lease is given up (#2572)."
    }
    CustomStringTableItem "{22205B2B89D3D298}" {
     Id "LF-Org_LeaseOurs"
     Target_en_us "Yours"
     Comment "Hideout lease status: this organization holds it."
    }
    CustomStringTableItem "{376956E573282A68}" {
     Id "LF-Org_LeaseRefused"
     Target_en_us "The lease was refused."
     Comment "Ledger notice: lease refused."
    }
    CustomStringTableItem "{3BCB98DA6F2D4899}" {
     Id "LF-Org_LeaseReleaseRefused"
     Target_en_us "Only a rank that may buy can give the lease up."
     Comment "Toast and ledger notice when giving up the lease is refused (#2572)."
    }
    CustomStringTableItem "{717EDA57B8114856}" {
     Id "LF-Org_LeaseTaken"
     Target_en_us "Leased elsewhere"
     Comment "Hideout lease status: another organization holds it."
    }
    CustomStringTableItem "{BD09FA58337E3B9B}" {
     Id "LF-Org_Leased"
     Target_en_us "The yard is yours. Members open its gate."
     Comment "Ledger notice after the lease."
    }
    CustomStringTableItem "{7B6C0A4DF16E499F}" {
     Id "LF-Org_Leave"
     Target_en_us "Leave"
     Comment "Ledger row action: leave the organization."
    }
    CustomStringTableItem "{5057D088D80BAF22}" {
     Id "LF-Org_LeaveRefused"
     Target_en_us "You cannot leave here."
     Comment "Ledger notice: leave refused."
    }
    CustomStringTableItem "{D4B253C21B4066E6}" {
     Id "LF-Org_LedgerTitle"
     Target_en_us "{Org} Register"
     Comment "Registry ledger page title."
    }
    CustomStringTableItem "{A26182C89E316E79}" {
     Id "LF-Org_Left"
     Target_en_us "You left the {org}."
     Comment "Ledger notice after leaving."
    }
    CustomStringTableItem "{22BBDF48A1B92971}" {
     Id "LF-Org_MembersLine"
     Target_en_us "Members (%1)"
     Comment "Ledger line above the member list. %1 is the count."
    }
    CustomStringTableItem "{EDD9EFE11266F42F}" {
     Id "LF-Org_Membership"
     Target_en_us "Your membership"
     Comment "Ledger row label."
    }
    CustomStringTableItem "{A8AB0A602B627AEA}" {
     Id "LF-Org_NameChars"
     Target_en_us "Use letters, digits, spaces, - ' & and . only."
     Comment "Founding refusal: characters not allowed."
    }
    CustomStringTableItem "{CF00BD73AD496C88}" {
     Id "LF-Org_NameEmpty"
     Target_en_us "Write a name first."
     Comment "Founding refusal: empty name."
    }
    CustomStringTableItem "{A74957BC3D56482F}" {
     Id "LF-Org_NameField"
     Target_en_us "Organization name"
     Comment "Ledger founding field placeholder."
    }
    CustomStringTableItem "{7F43F0ADE5BC0F97}" {
     Id "LF-Org_NameLine"
     Target_en_us "Organization: %1"
     Comment "Ledger line. %1 is the organization name."
    }
    CustomStringTableItem "{164B713160BF19CB}" {
     Id "LF-Org_NameLong"
     Target_en_us "The name fits 32 letters at most."
     Comment "Founding refusal: name too long."
    }
    CustomStringTableItem "{966C8BD11E837E8E}" {
     Id "LF-Org_NameShort"
     Target_en_us "The name needs at least 3 letters."
     Comment "Founding refusal: name too short."
    }
    CustomStringTableItem "{1C9D32A034633228}" {
     Id "LF-Org_NameTaken"
     Target_en_us "That name is already in the register."
     Comment "Founding refusal: duplicate name."
    }
    CustomStringTableItem "{10964C3EA631B4CE}" {
     Id "LF-Org_NewMember"
     Target_en_us "A new member joined %1."
     Comment "Toast to the inviter. %1 is the organization."
    }
    CustomStringTableItem "{1536EF0E4145400E}" {
     Id "LF-Org_NoCharacter"
     Target_en_us "Register your character at the {town} hall first."
     Comment "Ledger refusal: no character."
    }
    CustomStringTableItem "{BFB5F121CB45359D}" {
     Id "LF-Org_NoFee"
     Target_en_us "You do not carry the fee in cash."
     Comment "Founding refusal: not enough carried cash."
    }
    CustomStringTableItem "{BDDBD2D70EDA029F}" {
     Id "LF-Org_NotOwned"
     Target_en_us "Not owned"
     Comment "Ledger row status: upgrade not owned."
    }
    CustomStringTableItem "{6361E95B1AFC26BD}" {
     Id "LF-Org_OfferDeclined"
     Target_en_us "Your offer was turned down."
     Comment "Toast to the inviter: the invitee chose Decline."
    }
    CustomStringTableItem "{3E111C0C61E4B594}" {
     Id "LF-Org_OfferMade"
     Target_en_us "Offer made. They join by facing you and choosing Join."
     Comment "Toast to the inviter."
    }
    CustomStringTableItem "{7FCF2CD0A3A7C858}" {
     Id "LF-Org_OfferReceived"
     Target_en_us "You are offered a place in %1. Face them and choose Join."
     Comment "Toast to the invitee. %1 is the organization."
    }
    CustomStringTableItem "{BC1CC53C1A3EE577}" {
     Id "LF-Org_OfferRefused"
     Target_en_us "They cannot be invited."
     Comment "Toast to the inviter: offer refused."
    }
    CustomStringTableItem "{8FB7D90A7A0A7386}" {
     Id "LF-Org_Owned"
     Target_en_us "Owned"
     Comment "Ledger row status: upgrade owned."
    }
    CustomStringTableItem "{9051423983A8FD93}" {
     Id "LF-Org_Processing"
     Target_en_us "Processing upgrade"
     Comment "Ledger row label."
    }
    CustomStringTableItem "{A6563FE55129377C}" {
     Id "LF-Org_PromoteGeneric"
     Target_en_us "Promote"
     Comment "Interaction entry fallback: move a member one rank up."
    }
    CustomStringTableItem "{2814AA6686C5C43D}" {
     Id "LF-Org_PromoteIn"
     Target_en_us "Promote in %1"
     Comment "Interaction entry: a member with the manage-ranks right moves the member faced one rank up. %1 is the organization."
    }
    CustomStringTableItem "{2B01C1DAA65E369C}" {
     Id "LF-Org_RankFounder"
     Target_en_us "Founder"
     Comment "Rank name: founder."
    }
    CustomStringTableItem "{C476CC4110DB9C80}" {
     Id "LF-Org_RankGiven"
     Target_en_us "Rank given: %1."
     Comment "Toast to the one who promoted or demoted. %1 is the new rank."
    }
    CustomStringTableItem "{D52956E69B9B3735}" {
     Id "LF-Org_RankLine"
     Target_en_us "Your rank: %1"
     Comment "Ledger line. %1 is the translated rank."
    }
    CustomStringTableItem "{B9419ACE4881130A}" {
     Id "LF-Org_RankMember"
     Target_en_us "Member"
     Comment "Rank name: member."
    }
    CustomStringTableItem "{D2D275382EC82003}" {
     Id "LF-Org_RankOfficer"
     Target_en_us "Officer"
     Comment "Rank name: officer, between founder and member (#750a)."
    }
    CustomStringTableItem "{136AF5D9CB22A265}" {
     Id "LF-Org_RankRefused"
     Target_en_us "You cannot change their rank."
     Comment "Toast: the server refused a promotion or demotion."
    }
    CustomStringTableItem "{BCB1B5591518BDCE}" {
     Id "LF-Org_RemoveFrom"
     Target_en_us "Remove from %1"
     Comment "Interaction entry: a leader removes a member faced. %1 is the organization."
    }
    CustomStringTableItem "{774D14CCF0672CF3}" {
     Id "LF-Org_RemoveRefused"
     Target_en_us "You cannot remove them."
     Comment "Toast to the leader: removal refused."
    }
    CustomStringTableItem "{724D689AC7D42CE4}" {
     Id "LF-Org_Removed"
     Target_en_us "Member removed."
     Comment "Toast to the leader after a removal."
    }
    CustomStringTableItem "{EAD4C827F703D348}" {
     Id "LF-Org_SiteBase"
     Target_en_us "Base lease"
     Comment "Ledger held-ground list: mark beside the organisation base lease (#2834)."
    }
    CustomStringTableItem "{0E9BCB2663C907E5}" {
     Id "LF-Org_SiteCaptured"
     Target_en_us "Captured"
     Comment "Ledger held-ground list: mark beside a site the organisation took from a rival (#2834)."
    }
    CustomStringTableItem "{D7FA7CEB2B437576}" {
     Id "LF-Org_SitesLine"
     Target_en_us "Held ground (%1)"
     Comment "Ledger line above the held-site list. %1 is how many sites the organisation leases (#2834)."
    }
    CustomStringTableItem "{CC1FC9A0CFEB04C1}" {
     Id "LF-Org_TakeLease"
     Target_en_us "Take the lease"
     Comment "Ledger row action: lease the hideout yard."
    }
    CustomStringTableItem "{E69F55595D3DEC33}" {
     Id "LF-Org_Title"
     Target_en_us "{Org} register"
     Comment "Toast title for organization registry messages."
    }
    CustomStringTableItem "{F4D49F45A1FDC774}" {
     Id "LF-Org_Withdrawn"
     Target_en_us "Withdrawn from the fund."
     Comment "Ledger notice after a withdrawal."
    }
    CustomStringTableItem "{B97AC9C36C0F3C52}" {
     Id "LF-Org_YouWereRemoved"
     Target_en_us "You were removed from %1."
     Comment "Toast to the removed member. %1 is the organization."
    }
    CustomStringTableItem "{ECB3BBB991B9C5B9}" {
     Id "LF-Org_YourRank"
     Target_en_us "Your rank is now %1."
     Comment "Toast to the promoted or demoted member. %1 is the new rank."
    }
    CustomStringTableItem "{56655C954626CD1D}" {
     Id "LF-OutpostBoard_Hint"
     Target_en_us "Better pieces for friends of the camp"
     Comment "Last chalk line of the outpost board for a reader below the crate top tier. Never a tier or a number (N1)."
    }
    CustomStringTableItem "{5BD9E4FA0AF8FA6C}" {
     Id "LF-OutpostBoard_Next"
     Target_en_us "Ask at the crate beside the board."
     Comment "Sign catalog next text of the outpost stock board."
    }
    CustomStringTableItem "{7905DE54080B09B0}" {
     Id "LF-OutpostBoard_None"
     Target_en_us "Nothing on the shelf today"
     Comment "Chalk line of the outpost board when the crate has no stock rows."
    }
    CustomStringTableItem "{E4EC46DA39DBA91A}" {
     Id "LF-OutpostBoard_Read"
     Target_en_us "Read the board"
     Comment "Read row on the outpost chalk board: refreshes the lines painted on its face. No window opens."
    }
    CustomStringTableItem "{A7D148B678DA6EA2}" {
     Id "LF-OutpostBoard_Title"
     Target_en_us "Today's stock"
     Comment "Sign title of the chalk board beside the outpost crate (#4452)."
    }
    CustomStringTableItem "{ACDAC44B906974C2}" {
     Id "LF-OutpostBoard_What"
     Target_en_us "What the quartermaster has on hand today, with the price of each."
     Comment "Sign catalog what text of the outpost stock board (the face prints the live rows)."
    }
    CustomStringTableItem "{520C10CDD07CEBC1}" {
     Id "LF-OutpostBuyer_Full"
     Target_en_us "I have enough for now. Come back in about %1 minutes."
     Comment "Toast: the outpost buyer is at its demand cap (#4225). %1 = minutes until it takes another crate."
    }
    CustomStringTableItem "{52AFBF7DB477710B}" {
     Id "LF-OutpostBuyer_FullNever"
     Target_en_us "I have all the crates I need."
     Comment "Toast: the outpost buyer is full and never refills (#4225)."
    }
    CustomStringTableItem "{BC8A7CE6BEE6A4A4}" {
     Id "LF-OutpostBuyer_NoCrate"
     Target_en_us "You have nothing I want."
     Comment "Toast: the seller carries no contraband crate (#4225)."
    }
    CustomStringTableItem "{F87785E45E6D35AE}" {
     Id "LF-OutpostBuyer_NoRun"
     Target_en_us "Where did this come from? I only take crates that came up the road on a run."
     Comment "Toast: the seller has no running, driven convoy run to close (#4225)."
    }
    CustomStringTableItem "{42236C5858B2F6DA}" {
     Id "LF-OutpostBuyer_Sell"
     Target_en_us "Sell contraband crate (%1)"
     Comment "Action on the outpost buyer table (#4225). %1 = the price for the crate."
    }
    CustomStringTableItem "{FB3E7B610211031D}" {
     Id "LF-OutpostBuyer_Sold"
     Target_en_us "Good. Bring me more when you have it. You were paid %1."
     Comment "Toast after the outpost buyer takes crates (#4225). %1 = the cash paid."
    }
    CustomStringTableItem "{C615BD9428C48A36}" {
     Id "LF-OutpostBuyer_Title"
     Target_en_us "Rebel Buyer"
     Comment "Title of the outpost contraband buyer toasts (#4225)."
    }
    CustomStringTableItem "{87240FE872DFA583}" {
     Id "LF-OutpostBuyer_Unavailable"
     Target_en_us "Not now. Come back later."
     Comment "Toast: the outpost buyer could not complete the sale (#4225)."
    }
    CustomStringTableItem "{D52F08A3E91B7C46}" {
     Id "LF-OutpostStock_Bought"
     Target_en_us "Take it. Don't tell anyone where you got it. You paid %1."
     Comment "Toast after an outpost crate sale (#4223). %1 = the price paid."
    }
    CustomStringTableItem "{9B41E6D07C2A3F85}" {
     Id "LF-OutpostStock_Buy"
     Target_en_us "Buy %1 (%2)"
     Comment "Action on the quartermaster's crate (#4223). %1 = the item name, %2 = the price."
    }
    CustomStringTableItem "{F09A6E3C2D5B4718}" {
     Id "LF-OutpostStock_GangRadio"
     Target_en_us "gang radio"
     Comment "Item name on the outpost crate's Buy action (#4223)."
    }
    CustomStringTableItem "{E83B5D17A42C9F60}" {
     Id "LF-OutpostStock_NoCash"
     Target_en_us "Cash first. Come back when you have it."
     Comment "Toast: the buyer carries less cash than the outpost crate's price (#4223)."
    }
    CustomStringTableItem "{61C9A4F2B07E3D58}" {
     Id "LF-OutpostStock_NotKnown"
     Target_en_us "I don't know you well enough for that one. Come back when people talk about you."
     Comment "Toast: the buyer's underworld standing is below the item's (#4223). Never a number (decision N1)."
    }
    CustomStringTableItem "{B7D43E1A95C06F28}" {
     Id "LF-OutpostStock_Pistol"
     Target_en_us "pistol"
     Comment "Item name on the outpost crate's Buy action (#4223)."
    }
    CustomStringTableItem "{6ACB0A39142E7ED3}" {
     Id "LF-OutpostStock_PistolM9"
     Target_en_us "M9 pistol"
     Comment "Item name on an outpost crate's Buy action: the pack's stock tables (#4453)."
    }
    CustomStringTableItem "{4C1F8B29E6A37D05}" {
     Id "LF-OutpostStock_Rifle"
     Target_en_us "rifle"
     Comment "Item name on the outpost crate's Buy action (#4223)."
    }
    CustomStringTableItem "{9ACE1AA6CCD697C1}" {
     Id "LF-OutpostStock_RifleAks74u"
     Target_en_us "AKS-74U carbine"
     Comment "Item name on an outpost crate's Buy action: the pack's stock tables (#4453)."
    }
    CustomStringTableItem "{6D844291A8E12BEB}" {
     Id "LF-OutpostStock_RifleM16"
     Target_en_us "M16A2 rifle"
     Comment "Item name on an outpost crate's Buy action: the pack's stock tables (#4453)."
    }
    CustomStringTableItem "{3F7A2C91B4E05D18}" {
     Id "LF-OutpostStock_Title"
     Target_en_us "Quartermaster"
     Comment "Title of the outpost crate's toasts (#4223)."
    }
    CustomStringTableItem "{2A96F0C4D3B18E75}" {
     Id "LF-OutpostStock_Unavailable"
     Target_en_us "Not today."
     Comment "Toast: the outpost crate sale failed for another reason (no room, no item) (#4223)."
    }
    CustomStringTableItem "{6D41A0F3C8B2957E}" {
     Id "LF-Outpost_TraderRefusesOfficer"
     Target_en_us "We don't deal with the law here. Take the badge off and come back."
     Comment "Toast to an on-duty officer: a trader inside a rebel outpost refuses to serve them (#4217)."
    }
    CustomStringTableItem "{A3F59E1B7C402D86}" {
     Id "LF-Outpost_TraderRefusesOfficer_Title"
     Target_en_us "Not serving you"
     Comment "Title of the outpost trader's refusal toast to an on-duty officer (#4217)."
    }
    CustomStringTableItem "{5A9C30E7D1B84F62}" {
     Id "LF-Pager_NoPlate"
     Target_en_us "no plate"
     Comment "Pager stolen-car line when the car carries no plate (#4212)."
    }
    CustomStringTableItem "{4EF600B18D7A2C1B}" {
     Id "LF-Pager_None"
     Target_en_us "No calls paged."
     Comment "Pager card with no messages."
    }
    CustomStringTableItem "{9DC6DB6828971952}" {
     Id "LF-Pager_Read"
     Target_en_us "Read the pager"
     Comment "Action on a pager item (#249)."
    }
    CustomStringTableItem "{B7E1D4A92C06F385}" {
     Id "LF-Pager_StolenLine"
     Target_en_us "%1  Stolen car, plate %2: %3, reported at %4"
     Comment "Pager card line: an owner reported a car stolen (#4212). %1 = clock time HH:MM, %2 = plate, %3 = car name, %4 = place."
    }
    CustomStringTableItem "{CA566A486AE9A98D}" {
     Id "LF-Pager_Title"
     Target_en_us "Pager"
     Comment "Title of the pager card."
    }
    CustomStringTableItem "{117B8E27179EDAD3}" {
     Id "LF-Paper_DateLine"
     Target_en_us "Date %1"
     Comment "Date line of a printed slip. %1 is the in-game date and time."
    }
    CustomStringTableItem "{D625B9E68D445A88}" {
     Id "LF-Paper_DeliveryLine"
     Target_en_us "%1: %2 parcel(s), paid {currency:sym}%3"
     Comment "Delivery note line. %1 item, %2 amount, %3 pay."
    }
    CustomStringTableItem "{3D2C43C802AE9DE9}" {
     Id "LF-Paper_DeliveryTitle"
     Target_en_us "DELIVERY NOTE"
     Comment "Title line of a delivery note printed at a parcel hand-in (#4465)."
    }
    CustomStringTableItem "{0D96A126A3D70115}" {
     Id "LF-Paper_ItemGoods"
     Target_en_us "Goods"
     Comment "Item name on a weigh slip for a sale at a buyer."
    }
    CustomStringTableItem "{391EC1A04E968AA5}" {
     Id "LF-Paper_ItemGrain"
     Target_en_us "Grain"
     Comment "Item name on a weigh slip for a granary unload."
    }
    CustomStringTableItem "{597C99C21FB0FA59}" {
     Id "LF-Paper_ItemParcel"
     Target_en_us "Parcel"
     Comment "Item name on a delivery note."
    }
    CustomStringTableItem "{49A5BE10D89F3532}" {
     Id "LF-Paper_ItemShift"
     Target_en_us "Shift pay"
     Comment "Item name on a payslip."
    }
    CustomStringTableItem "{8F548416892E58B9}" {
     Id "LF-Paper_PayLine"
     Target_en_us "%1: %2 shift(s), paid {currency:sym}%3"
     Comment "Payslip line. %1 item, %2 amount, %3 pay."
    }
    CustomStringTableItem "{B026936CA93FA973}" {
     Id "LF-Paper_PayTitle"
     Target_en_us "PAYSLIP"
     Comment "Title line of a payslip printed after a paid shift (#4465)."
    }
    CustomStringTableItem "{D4F6FD7E91CBCA75}" {
     Id "LF-Paper_WeighLine"
     Target_en_us "%1: %2 lot(s), paid {currency:sym}%3"
     Comment "Weigh slip line. %1 item, %2 amount, %3 price paid."
    }
    CustomStringTableItem "{2C680CA6A955A645}" {
     Id "LF-Paper_WeighTitle"
     Target_en_us "WEIGH SLIP"
     Comment "Title line of a weigh slip printed at a buyer (#4465)."
    }
    CustomStringTableItem "{7D3A91E4C2B8F651}" {
     Id "LF-Parcel_DroppedNearby"
     Target_en_us "You dropped the parcel. It is on the ground nearby, marked with a glow."
     Comment "Toast when a carried courier parcel lands on the ground (#920)."
    }
    CustomStringTableItem "{7D3A91E4C2B8F650}" {
     Id "LF-Parcel_DroppedTitle"
     Target_en_us "Parcel"
     Comment "Toast title when a carried courier parcel lands on the ground."
    }
    CustomStringTableItem "{EBEE9355D5E41580}" {
     Id "LF-Payphone_Broken"
     Target_en_us "The payphone is out of order."
     Comment "Payphone: the call was refused for a reason the caller cannot fix."
    }
    CustomStringTableItem "{0D51EDC4BE6F1DDC}" {
     Id "LF-Payphone_CallMedic"
     Target_en_us "Ring for a doctor"
     Comment "Payphone action: ask for a medic."
    }
    CustomStringTableItem "{80B8D0AB91C5EBF6}" {
     Id "LF-Payphone_CallTaxi"
     Target_en_us "Ring for a cab"
     Comment "Payphone action: ask for a taxi."
    }
    CustomStringTableItem "{50F69750A6284C82}" {
     Id "LF-Payphone_Connected"
     Target_en_us "The call went through."
     Comment "Payphone toast: the call reached the service."
    }
    CustomStringTableItem "{33B2E99217263B93}" {
     Id "LF-Payphone_Cooldown"
     Target_en_us "The line is busy. Try again in a moment."
     Comment "Payphone: the player or the phone is cooling down."
    }
    CustomStringTableItem "{1490FFBE4947B795}" {
     Id "LF-Payphone_DescribeClothing"
     Target_en_us "Describe their clothing"
     Comment "Payphone action: a witness tells what the suspect wore."
    }
    CustomStringTableItem "{A21851D555A5F893}" {
     Id "LF-Payphone_DescribePlate"
     Target_en_us "Give a plate fragment"
     Comment "Payphone action: a witness reads part of the suspect vehicle plate."
    }
    CustomStringTableItem "{41736C02D6DB8859}" {
     Id "LF-Payphone_DescribeVehicle"
     Target_en_us "Describe their vehicle"
     Comment "Payphone action: a witness tells what the suspect drove."
    }
    CustomStringTableItem "{F75D3CCB7F1BFC4C}" {
     Id "LF-Payphone_NoCoin"
     Target_en_us "You need a coin for the payphone."
     Comment "Payphone: the player carries no cash for the coin."
    }
    CustomStringTableItem "{680515D680E0D9C0}" {
     Id "LF-Payphone_ReportCrime"
     Target_en_us "Report a crime"
     Comment "Payphone action: what happened is a crime."
    }
    CustomStringTableItem "{EE9B52E0D3888DB4}" {
     Id "LF-Payphone_ReportEmergency"
     Target_en_us "Report an emergency"
     Comment "Payphone action: what happened is an emergency."
    }
    CustomStringTableItem "{8E0EDAA6E007836E}" {
     Id "LF-Payphone_ReportSuspicious"
     Target_en_us "Report someone suspicious"
     Comment "Payphone action: what happened is suspicious behaviour."
    }
    CustomStringTableItem "{D0D0E6843F18276B}" {
     Id "LF-Payphone_Summary_Call"
     Target_en_us "Phoned the station"
     Comment "Summary the police radio, the dispatch board and the pagers show for a voice call to the station telephone."
    }
    CustomStringTableItem "{D219D04552F904F3}" {
     Id "LF-Payphone_Summary_Crime"
     Target_en_us "Crime reported"
     Comment "Summary the police radio line shows for a crime phoned in."
    }
    CustomStringTableItem "{367F9784CB23B15E}" {
     Id "LF-Payphone_Summary_Emergency"
     Target_en_us "Emergency reported"
     Comment "Summary the police radio line shows for an emergency phoned in."
    }
    CustomStringTableItem "{B680F53657359D11}" {
     Id "LF-Payphone_Summary_Medic"
     Target_en_us "Doctor wanted"
     Comment "Summary the medic pager and board show for a call from the payphone."
    }
    CustomStringTableItem "{C2AA364A6790F1CE}" {
     Id "LF-Payphone_Summary_Suspicious"
     Target_en_us "Suspicious person reported"
     Comment "Summary the police radio line shows for a suspicious person phoned in."
    }
    CustomStringTableItem "{4AA0B3E5926D13C3}" {
     Id "LF-Payphone_Summary_Taxi"
     Target_en_us "Cab wanted"
     Comment "Summary the taxi pager and board show for a call from the payphone."
    }
    CustomStringTableItem "{B49F143DD59203D0}" {
     Id "LF-Payphone_Title"
     Target_en_us "Payphone"
     Comment "Title of the payphone toast."
    }
    CustomStringTableItem "{7C9ACFE2F607ADE4}" {
     Id "LF-PerkPick_Change"
     Target_en_us "Change to %1 (%2), %3"
     Comment "Counter row: paid perk change. %1 = perk, %2 = trade, %3 = fee (#4416)"
    }
    CustomStringTableItem "{CE43CE20BD57EFD8}" {
     Id "LF-PerkPick_ChangeAny"
     Target_en_us "Change to %1, %2"
     Comment "Counter row before the client knows the trade: paid change. %1 = perk, %2 = fee (#4416)"
    }
    CustomStringTableItem "{9E6751E5C4E11EAB}" {
     Id "LF-PerkPick_Changed"
     Target_en_us "Perk changed. %1 paid."
     Comment "Toast: paid perk change done. %1 = fee (#4416)"
    }
    CustomStringTableItem "{E84CEFF083318DA3}" {
     Id "LF-PerkPick_Failed"
     Target_en_us "The clerk could not record that. Try again."
     Comment "Toast: pick failed (#4416)"
    }
    CustomStringTableItem "{1632EF2B9AA45FCB}" {
     Id "LF-PerkPick_NoMoney"
     Target_en_us "A perk change costs %1 in cash. You do not have enough."
     Comment "Toast: refused change, short of cash. %1 = fee (#4416)"
    }
    CustomStringTableItem "{534418E6E7F31E1B}" {
     Id "LF-PerkPick_NotOffered"
     Target_en_us "That perk is not open to you yet. Picks open at trade levels 10 and 20."
     Comment "Toast: refused pick (#4416)"
    }
    CustomStringTableItem "{604C99102F4E69D2}" {
     Id "LF-PerkPick_Perk_DoubleYield"
     Target_en_us "Double yield"
     Comment "Perk name (#4416)"
    }
  CustomStringTableItem "{F2E79F963D70D375}" {
   Id "LF-PerkPick_Perk_FastCuff"
   Target_en_us "Fast cuff"
   Comment "Police perk name: shorter cuff hold (#4573)"
  }
  CustomStringTableItem "{9D7CBBFEEB7AA1C6}" {
   Id "LF-PerkPick_Perk_GoodWord"
   Target_en_us "Good word"
   Comment "Perk name: traders pay 10% more for the trade's goods (#4573)"
  }
    CustomStringTableItem "{61E90E5710E9F92B}" {
     Id "LF-PerkPick_Perk_LightTouch"
     Target_en_us "Light touch"
     Comment "Perk name (#4416)"
    }
  CustomStringTableItem "{287C6C5C69D1FD5F}" {
   Id "LF-PerkPick_Perk_LongRadar"
   Target_en_us "Long radar"
   Comment "Police perk name: longer radar gun range (#4573)"
  }
  CustomStringTableItem "{0B655457D863937F}" {
   Id "LF-PerkPick_Perk_QuickAid"
   Target_en_us "Quick aid"
   Comment "Medic perk name: a dressing buys more time (#4573)"
  }
  CustomStringTableItem "{CC5C32C2C44180C2}" {
   Id "LF-PerkPick_Perk_QuickRevive"
   Target_en_us "Quick revive"
   Comment "Medic perk name: shorter revive hold (#4573)"
  }
  CustomStringTableItem "{E6943718899C1488}" {
   Id "LF-PerkPick_Perk_SteadyCarry"
   Target_en_us "Steady carry"
   Comment "Medic perk name: a carried patient bleeds slower (#4573)"
  }
    CustomStringTableItem "{3A444754F3E9577A}" {
     Id "LF-PerkPick_Perk_SwiftHands"
     Target_en_us "Swift hands"
     Comment "Perk name (#4416)"
    }
    CustomStringTableItem "{EF810CC51A4FD0EC}" {
     Id "LF-PerkPick_Take"
     Target_en_us "Take %1 (%2)"
     Comment "Counter row: free perk pick. %1 = perk, %2 = trade (#4416)"
    }
    CustomStringTableItem "{761DBAD47C3A4607}" {
     Id "LF-PerkPick_TakeAny"
     Target_en_us "Take %1"
     Comment "Counter row before the client knows the trade. %1 = perk (#4416)"
    }
    CustomStringTableItem "{C44E243F17A27F8E}" {
     Id "LF-PerkPick_Taken"
     Target_en_us "Perk taken. It works from your next shift."
     Comment "Toast: first perk pick done (#4416)"
    }
    CustomStringTableItem "{E670AFD3FEE77EB8}" {
     Id "LF-PerkPick_Title"
     Target_en_us "Labor office"
     Comment "Toast title at the labour office counter (#4416)"
    }
  CustomStringTableItem "{F15221208DB39DB9}" {
   Id "LF-PerkPick_TitleClinic"
   Target_en_us "Clinic"
   Comment "Toast title at the clinic dispatch board perk rows (#4573)"
  }
  CustomStringTableItem "{67F56523A9EC5CDA}" {
   Id "LF-PerkPick_TitlePolice"
   Target_en_us "{Police} station"
   Comment "Toast title at the police dispatch board perk rows (#4573)"
  }
    CustomStringTableItem "{3149C2DAEBE9569F}" {
     Id "LF-PerkPick_WeekLock"
     Target_en_us "You already changed this perk this week. Come back next week."
     Comment "Toast: refused change inside the week (#4416)"
    }
    CustomStringTableItem "{3CD776C95F970C59}" {
     Id "LF-Phone_Connected"
     Target_en_us "Someone picked up. The line is open."
     Comment "Toast to both talkers: the call was answered."
    }
    CustomStringTableItem "{B9F7280CFE2131EE}" {
     Id "LF-Phone_ConnectedTalk"
     Target_en_us "Someone picked up. Hold your radio key to talk."
     Comment "Toast to both talkers: the call was answered; the radio push-to-talk key now talks into the telephone."
    }
    CustomStringTableItem "{724AB80720A9B1ED}" {
     Id "LF-Phone_DialPolice"
     Target_en_us "Ring the police station"
     Comment "Payphone action: lift the receiver and ring the station telephone (a voice call)."
    }
    CustomStringTableItem "{647E1213753F5C49}" {
     Id "LF-Phone_HandsetDescription"
     Target_en_us "The receiver of the call you are on. It goes back when you hang up."
     Comment "Inventory description of the telephone receiver item."
    }
    CustomStringTableItem "{4E79AEAA1ECC6CF6}" {
     Id "LF-Phone_HandsetName"
     Target_en_us "Telephone receiver"
     Comment "Inventory name of the receiver a talker holds for the length of a call (the handset voice transport)."
    }
    CustomStringTableItem "{511F17E96A45A274}" {
     Id "LF-Phone_HangUp"
     Target_en_us "Hang up"
     Comment "Telephone action: put the receiver down and end the call."
    }
    CustomStringTableItem "{F4812934650FC09F}" {
     Id "LF-Phone_HungUp"
     Target_en_us "You put the receiver down."
     Comment "Toast to the talker who hung up."
    }
    CustomStringTableItem "{24435758A5CF028C}" {
     Id "LF-Phone_LeaveRequest"
     Target_en_us "Leave a message for a call back"
     Comment "Payphone action: after a call rang out or found nobody on duty, leave a call-back request (#147)."
    }
    CustomStringTableItem "{A3F52DC9AD380D5E}" {
     Id "LF-Phone_LineBusy"
     Target_en_us "Busy signal. The station line is engaged."
     Comment "Telephone refusal: every station telephone of the service is already on a call."
    }
    CustomStringTableItem "{E465752BE2E0AD2D}" {
     Id "LF-Phone_LineDead"
     Target_en_us "The line went dead."
     Comment "Toast to a talker: the other end hung up, walked off, went down or left."
    }
    CustomStringTableItem "{BC90AE92BE14E820}" {
     Id "LF-Phone_NoAnswer"
     Target_en_us "Nobody picked up."
     Comment "Toast to the caller: the call rang out unanswered."
    }
    CustomStringTableItem "{2F6EBB4F95048920}" {
     Id "LF-Phone_NoAnswerOffer"
     Target_en_us "Nobody picked up. Leave a message here and the station will ring back."
     Comment "Toast to the caller: the call rang out; a call-back request may be left at that payphone."
    }
    CustomStringTableItem "{E141F35D692DD238}" {
     Id "LF-Phone_NoLine"
     Target_en_us "The line is dead. No telephone answers that number here."
     Comment "Telephone refusal: no station telephone for the service exists in this world."
    }
    CustomStringTableItem "{C4782A22D7EDB5DA}" {
     Id "LF-Phone_NoOffer"
     Target_en_us "You have no missed call from this telephone to leave a message for."
     Comment "Refusal: the player has no call-back offer at this payphone (no missed call, or the offer ran out)."
    }
    CustomStringTableItem "{3D04C4CC444D598D}" {
     Id "LF-Phone_NobodyOffer"
     Target_en_us "Nobody is at the station. Leave a message here and they will ring back."
     Comment "Toast to the caller: nobody is on duty; a call-back request may be left at that payphone."
    }
    CustomStringTableItem "{BCE2B941D51071CA}" {
     Id "LF-Phone_NotOnDuty"
     Target_en_us "Only an officer on duty answers the station telephone."
     Comment "Telephone refusal: the player is not on duty for the service of this station telephone."
    }
    CustomStringTableItem "{1631B276C9DBC6BF}" {
     Id "LF-Phone_NotRinging"
     Target_en_us "This telephone is not ringing."
     Comment "Telephone refusal: picked up a telephone with no call on it."
    }
    CustomStringTableItem "{EB9C965AE30ACFF5}" {
     Id "LF-Phone_NotYourCall"
     Target_en_us "You are not on this line."
     Comment "Telephone refusal: hung up a telephone the player is not talking on."
    }
    CustomStringTableItem "{BA03409293F966F8}" {
     Id "LF-Phone_OnACall"
     Target_en_us "You are already on a call."
     Comment "Telephone refusal: the player is talking on another call."
    }
    CustomStringTableItem "{F95D32253E487FB1}" {
     Id "LF-Phone_PadCallBack"
     Target_en_us "Ring back"
     Comment "Pad line button: ring the payphone the request came from."
    }
    CustomStringTableItem "{6B69BDA7166DD69D}" {
     Id "LF-Phone_PadCalling"
     Target_en_us "%1 (ringing back)"
     Comment "Pad line status while someone rings that payphone back. %1 is the message."
    }
    CustomStringTableItem "{42FDE23CF0AC3481}" {
     Id "LF-Phone_PadClose"
     Target_en_us "Put the pad down"
     Comment "Station message pad close button."
    }
    CustomStringTableItem "{D0F1DC66613DA0C1}" {
     Id "LF-Phone_PadHint"
     Target_en_us "Call-back requests left at payphones. Ring one back from this telephone."
     Comment "Hint line on the station message pad."
    }
    CustomStringTableItem "{5BD8A3BD5B8F473D}" {
     Id "LF-Phone_PadLine"
     Target_en_us "%1  %2, rang %3 s"
     Comment "One request on the pad. %1 the in-game time it was left, %2 the place of the payphone, %3 seconds it rang."
    }
    CustomStringTableItem "{DE961B312ED56923}" {
     Id "LF-Phone_PadNoMessage"
     Target_en_us "No message"
     Comment "Pad line status: the caller typed nothing."
    }
    CustomStringTableItem "{2FE54F022DB5CA14}" {
     Id "LF-Phone_PadNone"
     Target_en_us "No messages."
     Comment "Station message pad with no call-back requests."
    }
    CustomStringTableItem "{C2EB7CAD98723360}" {
     Id "LF-Phone_PadNotOnDuty"
     Target_en_us "Only an officer on duty reads the station pad."
     Comment "Refusal: the player is not on duty for the station telephone's service."
    }
    CustomStringTableItem "{7AA1CB1952D2EA8E}" {
     Id "LF-Phone_PadTitle"
     Target_en_us "While you were out"
     Comment "Title of the station message pad (the period call-back slip)."
    }
    CustomStringTableItem "{CD75E1603F385300}" {
     Id "LF-Phone_PhoneBusy"
     Target_en_us "This telephone is in use."
     Comment "Telephone refusal: someone else is using this telephone."
    }
    CustomStringTableItem "{9D830AA1DCE7D2B2}" {
     Id "LF-Phone_PickUp"
     Target_en_us "Pick up the receiver"
     Comment "Telephone action: answer a ringing telephone."
    }
    CustomStringTableItem "{DF8387B5D8AB3DA0}" {
     Id "LF-Phone_ReadPad"
     Target_en_us "Read the message pad"
     Comment "Station telephone action: list the call-back requests left at payphones."
    }
    CustomStringTableItem "{257CE64B80315E01}" {
     Id "LF-Phone_RequestCalling"
     Target_en_us "Someone is already ringing that telephone back."
     Comment "Refusal: another officer is ringing this request back."
    }
    CustomStringTableItem "{DF38F94D73120112}" {
     Id "LF-Phone_RequestGone"
     Target_en_us "That message is no longer on the pad."
     Comment "Refusal: the call-back request expired, was answered, or belongs to another service."
    }
    CustomStringTableItem "{E48722D4B403267E}" {
     Id "LF-Phone_RequestLeft"
     Target_en_us "Message left. The station will ring this telephone back."
     Comment "Toast to the caller: the call-back request is on the station pad."
    }
    CustomStringTableItem "{C42B50D4E2C64F7C}" {
     Id "LF-Phone_RequestOnPad"
     Target_en_us "A call-back request is on the station pad."
     Comment "Toast to everyone on duty for the service: a caller left a call-back request."
    }
    CustomStringTableItem "{8D1F5DAFD398EAF5}" {
     Id "LF-Phone_RequestTooLong"
     Target_en_us "The message is too long."
     Comment "Refusal: the server found the typed message longer than the configured limit."
    }
    CustomStringTableItem "{2A96C07D4D496947}" {
     Id "LF-Phone_RequestTooLongBy"
     Target_en_us "Too long by %1 characters."
     Comment "Message slip error line. %1 is how many characters to cut."
    }
    CustomStringTableItem "{AF38AF3C3E652AC0}" {
     Id "LF-Phone_Ringing"
     Target_en_us "It is ringing at the other end."
     Comment "Toast to the caller: the call is ringing at the station."
    }
    CustomStringTableItem "{C5B665DD46E8ECE5}" {
     Id "LF-Phone_RingingBack"
     Target_en_us "The station is ringing back the telephone you called from."
     Comment "Toast to the caller who left a request: an officer rings that payphone now."
    }
    CustomStringTableItem "{87FE88A4ADBE1096}" {
     Id "LF-Phone_SlipCancel"
     Target_en_us "Hang up"
     Comment "Message slip close button: leave no request."
    }
    CustomStringTableItem "{256554EEA5E89F45}" {
     Id "LF-Phone_SlipField"
     Target_en_us "Message (up to %1 characters)"
     Comment "Label of the message field on the slip. %1 is the length limit."
    }
    CustomStringTableItem "{95315859351CA873}" {
     Id "LF-Phone_SlipHint"
     Target_en_us "The station will ring this telephone back. A few words help; you may leave none."
     Comment "Hint line on the message slip."
    }
    CustomStringTableItem "{12ED03DE459A7FC8}" {
     Id "LF-Phone_SlipLeave"
     Target_en_us "Leave the message"
     Comment "Message slip button: send the call-back request."
    }
    CustomStringTableItem "{BC2E18872DCB8DB9}" {
     Id "LF-Phone_SlipTitle"
     Target_en_us "Message for the station"
     Comment "Title of the message slip a caller fills in at a payphone."
    }
    CustomStringTableItem "{FDD324D5C2537635}" {
     Id "LF-Phone_StationRinging"
     Target_en_us "The station telephone is ringing."
     Comment "Toast to everyone on duty for the service: a call is ringing at the station telephone."
    }
    CustomStringTableItem "{329AA4071D9D4E53}" {
     Id "LF-Phone_StationTitle"
     Target_en_us "Station telephone"
     Comment "Description line of the station telephone's actions."
    }
    CustomStringTableItem "{12ACDFB2A237C258}" {
     Id "LF-Phone_TargetBusy"
     Target_en_us "Busy signal. That telephone is in use."
     Comment "Refusal: the payphone a call-back request came from is in use; try again later."
    }
    CustomStringTableItem "{E7D83B6B552680AE}" {
     Id "LF-Phone_Title"
     Target_en_us "Telephone"
     Comment "Title of every telephone toast (#147)."
    }
    CustomStringTableItem "{458C896F6A964243}" {
     Id "LF-Phone_TooFar"
     Target_en_us "Stand at the telephone to use it."
     Comment "Telephone refusal: the server found the player too far from the telephone."
    }
    CustomStringTableItem "{C7D5CA3226CB67B4}" {
     Id "LF-Phone_TooLong"
     Target_en_us "The exchange cut the call. It ran too long."
     Comment "Toast to both talkers: the call reached the configured maximum length."
    }
    CustomStringTableItem "{4C1C8D5F5970AE54}" {
     Id "LF-Phone_VonEntry"
     Target_en_us "Telephone"
     Comment "Name of the telephone entry in the voice menu and on the voice hint while a call is connected (#147)."
    }
    CustomStringTableItem "{F5AAE9C32EB617FE}" {
     Id "LF-Phone_WalkedAway"
     Target_en_us "You walked away from the telephone. The call is over."
     Comment "Toast to a talker who went further than the receiver cord."
    }
    CustomStringTableItem "{42A73AE4842C1045}" {
     Id "LF-Placement_Aim"
     Target_en_us "Place where I aim"
     Comment "Action name on a placeable item: enters aimed placement mode, the ghost follows the aim (#1028)."
    }
    CustomStringTableItem "{0C64EF85E0E4C2F5}" {
     Id "LF-Placement_AimHint"
     Target_en_us "Enter or A place   Backspace or B cancel   [ and ] turn   Shift or right stick fine"
     Comment "Hint line while the placement ghost follows the aim: the keys (#1028)."
    }
    CustomStringTableItem "{5B1D9E0A7C3F4A21}" {
     Id "LF-Placement_AimTitle"
     Target_en_us "Placing"
     Comment "Title of the aimed placement hint (#1028)."
    }
    CustomStringTableItem "{F4BBE2F164436809}" {
     Id "LF-Placement_Blocked"
     Target_en_us "Something is in the way."
     Comment "Toast when a placement is refused because something stands where it would go: a car, a post, a spike strip already lying there (#928)."
    }
    CustomStringTableItem "{1C32E2DFC53A052C}" {
     Id "LF-Placement_CapOwner"
     Target_en_us "You already have as many things placed as you may keep. Pick one up first."
     Comment "Toast when a permanent placement is refused: the player reached the per-player cap (#1032)."
    }
    CustomStringTableItem "{0E908C127EF9F835}" {
     Id "LF-Placement_CapProperty"
     Target_en_us "This property already holds as many placed things as it may. Pick one up first."
     Comment "Toast when a permanent placement is refused: the property reached its cap (#1032)."
    }
    CustomStringTableItem "{4A63282A460A441D}" {
     Id "LF-Placement_EmptyFirst"
     Target_en_us "Empty it first"
     Comment "Toast under Placing when the player picks up a storage furniture that still holds items."
    }
    CustomStringTableItem "{7D3F102C9E5B6C43}" {
     Id "LF-Placement_KeyCancel"
     Target_en_us "Cancel placing"
     Comment "Key binding name: leave aimed placement (#1028)."
    }
    CustomStringTableItem "{A062435FC18E9F76}" {
     Id "LF-Placement_KeyFine"
     Target_en_us "Turn placement finely (hold)"
     Comment "Key binding name: hold to rotate the placement ghost in 1 degree steps (#1028)."
    }
    CustomStringTableItem "{6C2E0F1B8D4A5B32}" {
     Id "LF-Placement_KeyPlace"
     Target_en_us "Place item"
     Comment "Key binding name: confirm the placement ghost (#1028)."
    }
    CustomStringTableItem "{8E40213DAF6C7D54}" {
     Id "LF-Placement_KeyTurnLeft"
     Target_en_us "Turn placement left"
     Comment "Key binding name: rotate the placement ghost left (#1028)."
    }
    CustomStringTableItem "{9F51324EB07D8E65}" {
     Id "LF-Placement_KeyTurnRight"
     Target_en_us "Turn placement right"
     Comment "Key binding name: rotate the placement ghost right (#1028)."
    }
    CustomStringTableItem "{284442A5E34D1270}" {
     Id "LF-Placement_Moved"
     Target_en_us "You moved. Look at the spot again and retry."
     Comment "Toast when the spot the player's machine proposed is too far from where the server sees them place it (#1026)."
    }
    CustomStringTableItem "{F2BDAD6DBBEFE3E2}" {
     Id "LF-Placement_NeedBase"
     Target_en_us "You can only place this at your organization's base."
     Comment "Toast when the placement policy refuses: the kind needs the organization's base nearby (#2447)."
    }
    CustomStringTableItem "{2448C0A51B7E4D01}" {
     Id "LF-Placement_NeedCash"
     Target_en_us "You don't have enough cash to place that."
     Comment "Toast when a placement is refused because the player carries less cash than the kind costs (#2448)."
    }
    CustomStringTableItem "{A1D7C4E09B3F5202}" {
     Id "LF-Placement_NeedHouse"
     Target_en_us "Only the owner or a key holder of a house nearby can place this."
     Comment "Toast when the placement policy refuses: the kind needs the house owner or a key holder (#2446)."
    }
    CustomStringTableItem "{2448C0A51B7E4D02}" {
     Id "LF-Placement_NeedItem"
     Target_en_us "You don't have what it takes to place that."
     Comment "Toast when a placement is refused because the player lacks the items the kind uses up (#2448)."
    }
    CustomStringTableItem "{A1D7C4E09B3F5203}" {
     Id "LF-Placement_NeedOrg"
     Target_en_us "Only a member of an organization can place this."
     Comment "Toast when the placement policy refuses: the kind needs an organization member (#2446)."
    }
    CustomStringTableItem "{A1D7C4E09B3F5201}" {
     Id "LF-Placement_NeedPolice"
     Target_en_us "Only on-duty {police} can place this."
     Comment "Toast when the placement policy refuses: the kind needs an on-duty officer (#2446)."
    }
    CustomStringTableItem "{B4CD02854FD9C6F2}" {
     Id "LF-Placement_NeedProperty"
     Target_en_us "You can only place this inside a property you own or hold a key to."
     Comment "Toast when the placement policy refuses: the kind needs an owned or keyed property (#2447)."
    }
    CustomStringTableItem "{A1D7C4E09B3F5204}" {
     Id "LF-Placement_NeedRank"
     Target_en_us "Your organization rank is too low to place this."
     Comment "Toast when the placement policy refuses: the kind needs a higher organization rank (#2446)."
    }
    CustomStringTableItem "{7D3AE2EE9C995350}" {
     Id "LF-Placement_NeedRoad"
     Target_en_us "You can only place this on a road."
     Comment "Toast when the placement policy refuses: the kind may go on roads only (#2447)."
    }
    CustomStringTableItem "{70A8229A6F903FA6}" {
     Id "LF-Placement_NoGround"
     Target_en_us "There is no flat ground there."
     Comment "Toast when a placement is refused because the ground under it is missing or rises too much (a kerb, a slope) (#1026)."
    }
    CustomStringTableItem "{CC9466F58FD8A430}" {
     Id "LF-Placement_NotAllowed"
     Target_en_us "You are not allowed to place this."
     Comment "Toast when the placement service refuses because the player may not place that item: a spike strip off duty (#1026)."
    }
    CustomStringTableItem "{3E6A1F5C92D80B47}" {
     Id "LF-Placement_PickUp"
     Target_en_us "Pick up"
     Comment "Action name on a permanent prop its owner placed (a chair, a table): takes it back into the inventory (#2751)."
    }
    CustomStringTableItem "{42E27FF2B0C5A49F}" {
     Id "LF-Placement_Title"
     Target_en_us "Placing"
     Comment "Toast title for every placement refusal (#1026)."
    }
    CustomStringTableItem "{35DC52C82B0BDC1E}" {
     Id "LF-Plant_Action_Harvest"
     Target_en_us "Harvest plant"
     Comment "Action: harvest the nearest ripe plant, anyone may (#2664)."
    }
    CustomStringTableItem "{2C28995D979CC096}" {
     Id "LF-Plant_Action_Inspect"
     Target_en_us "Inspect plant"
     Comment "Action: look at the nearest plant (#2664)."
    }
    CustomStringTableItem "{5E937DD36134DE70}" {
     Id "LF-Plant_Action_Plant"
     Target_en_us "Plant seed"
     Comment "Action on a seed item: plant it on the ground in front of you (#2664)."
    }
    CustomStringTableItem "{438D69BE8666C5A2}" {
     Id "LF-Plant_Action_Water"
     Target_en_us "Water plant"
     Comment "Action on a water container: water the nearest plant (#2664)."
    }
    CustomStringTableItem "{91F3B6D2C0E8A474}" {
     Id "LF-Plant_CharacterCap"
     Target_en_us "You are already tending as many plants as you can."
     Comment "Toast: the character has reached the per-character plant cap (12)."
    }
    CustomStringTableItem "{F4CF5DF8731FD112}" {
     Id "LF-Plant_Cleared"
     Target_en_us "You pull up the dead plant."
     Comment "Toast: a dead or rotten plant was cleared with nothing to pick (#2664)."
    }
    CustomStringTableItem "{D1CC92B9AB0144A9}" {
     Id "LF-Plant_GrantFailed"
     Target_en_us "The harvest could not be picked."
     Comment "Toast: harvest grant failed, the plant stays (#2664)."
    }
    CustomStringTableItem "{2B96D1F4A8C03E57}" {
     Id "LF-Plant_InVehicle"
     Target_en_us "You can't plant from a vehicle."
     Comment "Toast: planting is refused while the player is in a vehicle."
    }
    CustomStringTableItem "{A45CF761290D0E7F}" {
     Id "LF-Plant_NoCrop"
     Target_en_us "That does not grow here."
     Comment "Toast: the item is not a seed of any crop, or the crop was removed (#2664)."
    }
    CustomStringTableItem "{441EE7136209D33C}" {
     Id "LF-Plant_NoPlant"
     Target_en_us "There is no plant close enough."
     Comment "Toast: no plant within reach (#2664)."
    }
    CustomStringTableItem "{EBB828FC04116312}" {
     Id "LF-Plant_NoSeed"
     Target_en_us "You have no seed of that crop."
     Comment "Toast: planting refused, the seed is not in the inventory (#2664)."
    }
    CustomStringTableItem "{60E32823B695604A}" {
     Id "LF-Plant_NoWater"
     Target_en_us "You need something with water in it."
     Comment "Toast: watering refused, no water container carried (#2664)."
    }
    CustomStringTableItem "{1249E5766896037C}" {
     Id "LF-Plant_NotGrowing"
     Target_en_us "It is past watering now."
     Comment "Toast: watering refused, the plant is ripe, dead or rotten (#2664)."
    }
    CustomStringTableItem "{5C2E81D9A47B03F6}" {
     Id "LF-Plant_NotOpenGround"
     Target_en_us "Seeds only go in open ground: not indoors, under cover, on a road or in water."
     Comment "Toast: planting refused, the spot is inside or under a building, on a road or below sea level (#2821)."
    }
    CustomStringTableItem "{AFB196EB146950F9}" {
     Id "LF-Plant_NotRipe"
     Target_en_us "It is not ripe yet."
     Comment "Toast: harvest refused, still growing (#2664)."
    }
    CustomStringTableItem "{97A6F835BC560561}" {
     Id "LF-Plant_NotThirsty"
     Target_en_us "The soil is still damp."
     Comment "Toast: watering refused, too early since the last watering (#2664)."
    }
    CustomStringTableItem "{E4A07C5B1D3F9268}" {
     Id "LF-Plant_SafeZone"
     Target_en_us "Nothing grows in the safe zone."
     Comment "Toast: planting is refused inside a safe zone."
    }
    CustomStringTableItem "{A8D5E2073B1C96F0}" {
     Id "LF-Plant_ServerCap"
     Target_en_us "The land is full; no more plants fit right now."
     Comment "Toast: the server-wide plant cap (300) is reached."
    }
    CustomStringTableItem "{189E1B35DD800939}" {
     Id "LF-Plant_Stage_Dead"
     Target_en_us "Dried out and dead."
     Comment "Inspect toast: the plant missed its watering and died (#2664)."
    }
    CustomStringTableItem "{822A530AEEA855B0}" {
     Id "LF-Plant_Stage_Growing"
     Target_en_us "Growing well."
     Comment "Inspect toast: the plant is past halfway to ripe (#2664)."
    }
    CustomStringTableItem "{A903ED7A9B6712D2}" {
     Id "LF-Plant_Stage_Ripe"
     Target_en_us "Ripe and ready to pick."
     Comment "Inspect toast: the plant can be harvested (#2664)."
    }
    CustomStringTableItem "{357CED2AB6738715}" {
     Id "LF-Plant_Stage_Rotten"
     Target_en_us "Left too long, it has rotted."
     Comment "Inspect toast: the ripe plant was not picked in time (#2664)."
    }
    CustomStringTableItem "{4EF802B73F17EF25}" {
     Id "LF-Plant_Stage_Sprout"
     Target_en_us "A small sprout."
     Comment "Inspect toast: the plant has just come up (#2664)."
    }
    CustomStringTableItem "{33116CD2BFBA424C}" {
     Id "LF-Plant_Title"
     Target_en_us "Garden"
     Comment "Toast title for planting, watering, harvesting and inspecting a free crop (#2664)."
    }
    CustomStringTableItem "{5C1E8A3D70B94F21}" {
     Id "LF-Plant_TooClose"
     Target_en_us "Too close to another plant."
     Comment "Toast: a seed cannot be planted within 1.5 m of another plant."
    }
    CustomStringTableItem "{7D2F4B90E3A618C5}" {
     Id "LF-Plant_TooFar"
     Target_en_us "Too far away to plant there."
     Comment "Toast: the spot is beyond the 4 m planting reach."
    }
    CustomStringTableItem "{8FD7B9CC66956694}" {
     Id "LF-Plant_Water_Fine"
     Target_en_us "The soil is still damp."
     Comment "Inspect toast: the plant does not need water yet (#2664)."
    }
    CustomStringTableItem "{36802946AA27D91E}" {
     Id "LF-Plant_Water_Thirsty"
     Target_en_us "The soil is dry. It needs water."
     Comment "Inspect toast: the plant can be watered now (#2664)."
    }
    CustomStringTableItem "{570FA0D843201D55}" {
     Id "LF-PlayerMenuFile_BackInDays"
     Target_en_us "Back in %1 days"
     Comment "Personal file My file tab: days left before you can get a seized license again. %1 = in-game days."
    }
    CustomStringTableItem "{7A5439052B9C5DB6}" {
     Id "LF-PlayerMenuFile_BookedDaysAgo"
     Target_en_us "%1 days ago"
     Comment "Personal file My file tab: the latest booking was %1 in-game days ago (2 or more)."
    }
    CustomStringTableItem "{856FE81A052274CE}" {
     Id "LF-PlayerMenuFile_BookedToday"
     Target_en_us "Today"
     Comment "Personal file My file tab: the latest booking was today."
    }
    CustomStringTableItem "{C34A076E3328C276}" {
     Id "LF-PlayerMenuFile_BookedYesterday"
     Target_en_us "Yesterday"
     Comment "Personal file My file tab: the latest booking was yesterday."
    }
    CustomStringTableItem "{B55AA2D7B6581EFC}" {
     Id "LF-PlayerMenuFile_BookingLatest"
     Target_en_us "Latest"
     Comment "Personal file My file tab: label of when the latest booking was."
    }
    CustomStringTableItem "{F481BAAD9F51B93E}" {
     Id "LF-PlayerMenuFile_BookingsNone"
     Target_en_us "No bookings on your record."
     Comment "Personal file My file tab: you were never booked."
    }
    CustomStringTableItem "{37813A89028CFD07}" {
     Id "LF-PlayerMenuFile_BookingsOnRecord"
     Target_en_us "On your record"
     Comment "Personal file My file tab: label of the count of bookings."
    }
    CustomStringTableItem "{2E215005E56BCC7F}" {
     Id "LF-PlayerMenuFile_HeadBookings"
     Target_en_us "BOOKINGS"
     Comment "Personal file My file tab: card heading, times you were booked at the station."
    }
    CustomStringTableItem "{5CECE9A37CD53994}" {
     Id "LF-PlayerMenuFile_HeadLicences"
     Target_en_us "SEIZED LICENSES"
     Comment "Personal file My file tab: card heading, licenses taken from you."
    }
    CustomStringTableItem "{4B7202437ACFD27E}" {
     Id "LF-PlayerMenuFile_HeadTickets"
     Target_en_us "OPEN TICKETS"
     Comment "Personal file My file tab: card heading, tickets you have not paid."
    }
    CustomStringTableItem "{1B6D7F751FDA116C}" {
     Id "LF-PlayerMenuFile_LicenceDriving"
     Target_en_us "Driver license"
     Comment "Personal file My file tab: name of the seized driving license."
    }
    CustomStringTableItem "{6EB166187A523AF2}" {
     Id "LF-PlayerMenuFile_LicenceFirearm"
     Target_en_us "Firearm permit"
     Comment "Personal file My file tab: name of the seized firearm permit."
    }
    CustomStringTableItem "{57DD9D56B0F920CF}" {
     Id "LF-PlayerMenuFile_LicenceOther"
     Target_en_us "License"
     Comment "Personal file My file tab: name of any other seized license."
    }
    CustomStringTableItem "{777ED663245DE584}" {
     Id "LF-PlayerMenuFile_LicencesNone"
     Target_en_us "No license has been seized."
     Comment "Personal file My file tab: nothing was taken from you."
    }
    CustomStringTableItem "{D5ABF2EC95BB65EE}" {
     Id "LF-PlayerMenuFile_RecordBookedBy"
     Target_en_us "Booked by %1"
     Comment "Personal file My file tab: the officer who booked you. %1 = the officer's character name."
    }
    CustomStringTableItem "{2A06F14E3034979E}" {
     Id "LF-PlayerMenuFile_RecordLabel"
     Target_en_us "%1: %2"
     Comment "Personal file My file tab: one past booking. %1 = when (Today, Yesterday, N days ago), %2 = the charges, comma separated."
    }
    CustomStringTableItem "{80D3060B42A68C40}" {
     Id "LF-PlayerMenuFile_RecordNoCharge"
     Target_en_us "No charge listed"
     Comment "Personal file My file tab: a past booking whose sheet named no charge."
    }
    CustomStringTableItem "{00258895E7B9005A}" {
     Id "LF-PlayerMenuFile_RecordPaid"
     Target_en_us "%1 paid"
     Comment "Personal file My file tab: the booking was settled by paying the fine at the desk. %1 = the amount, e.g. $400."
    }
    CustomStringTableItem "{245FFCAB2E8E8F7E}" {
     Id "LF-PlayerMenuFile_RecordSeized"
     Target_en_us "%1 seized"
     Comment "Personal file My file tab: a license the booking took. %1 = the license name, e.g. Driver license."
    }
    CustomStringTableItem "{EAAC6BFC80D08683}" {
     Id "LF-PlayerMenuFile_RecordServed"
     Target_en_us "%1 min served"
     Comment "Personal file My file tab: the booking was settled in the cell. %1 = minutes."
    }
    CustomStringTableItem "{573CBFF3D455E78C}" {
     Id "LF-PlayerMenuFile_TicketLabel"
     Target_en_us "Ticket %1: %2"
     Comment "Personal file My file tab: one ticket row. %1 = ticket number, %2 = the offence."
    }
    CustomStringTableItem "{72B70C6B7BC67C28}" {
     Id "LF-PlayerMenuFile_TicketsNone"
     Target_en_us "No open tickets."
     Comment "Personal file My file tab: you owe nothing on a ticket."
    }
    CustomStringTableItem "{450FC8E2C44F3CBF}" {
     Id "LF-PlayerMenuFile_Waiting"
     Target_en_us "Pulling your file..."
     Comment "Personal file My file tab: shown until the server answers."
    }
    CustomStringTableItem "{913159AB574CA2A1}" {
     Id "LF-PlayerMenuGang_Flag"
     Target_en_us "Flag"
     Comment "Personal file Gang tab: label of the gang's flag; the value is the flag's name."
    }
    CustomStringTableItem "{234FCA6DAAD47422}" {
     Id "LF-PlayerMenuGang_HeadCrew"
     Target_en_us "YOUR CREW"
     Comment "Personal file Gang tab: card heading, the gang's name, flag, your rank and radio."
    }
    CustomStringTableItem "{EB3CACE7C34B7736}" {
     Id "LF-PlayerMenuGang_HeadOnline"
     Target_en_us "ONLINE NOW"
     Comment "Personal file Gang tab: card heading, the gang's members online."
    }
    CustomStringTableItem "{0345AA03F2A3446B}" {
     Id "LF-PlayerMenuGang_HeadSites"
     Target_en_us "SITES HELD"
     Comment "Personal file Gang tab: card heading, the sites the gang holds."
    }
    CustomStringTableItem "{7B5972DECE2933EB}" {
     Id "LF-PlayerMenuGang_Name"
     Target_en_us "Gang"
     Comment "Personal file Gang tab: label of the gang's name."
    }
    CustomStringTableItem "{A5BAEB0ED177CCF0}" {
     Id "LF-PlayerMenuGang_NoFlag"
     Target_en_us "No flag"
     Comment "Personal file Gang tab: the gang flies no flag."
    }
    CustomStringTableItem "{103BF10661BA7414}" {
     Id "LF-PlayerMenuGang_OnlineNone"
     Target_en_us "Nobody is online."
     Comment "Personal file Gang tab: no member of the gang is online."
    }
    CustomStringTableItem "{B1BF19836FF7ED42}" {
     Id "LF-PlayerMenuGang_Radio"
     Target_en_us "Radio"
     Comment "Personal file Gang tab: label of the radio line."
    }
    CustomStringTableItem "{079B74DA2C54DDA2}" {
     Id "LF-PlayerMenuGang_RadioNone"
     Target_en_us "No radio carried"
     Comment "Personal file Gang tab: the member carries no radio."
    }
    CustomStringTableItem "{24B4149B9DCB481E}" {
     Id "LF-PlayerMenuGang_RadioOff"
     Target_en_us "Not on the gang channel"
     Comment "Personal file Gang tab: the member carries a radio that is not on the gang channel."
    }
    CustomStringTableItem "{677C3AE5B1C69820}" {
     Id "LF-PlayerMenuGang_RadioOn"
     Target_en_us "On the gang channel"
     Comment "Personal file Gang tab: a radio the member carries is tuned to the gang channel."
    }
    CustomStringTableItem "{38497A994749DBCA}" {
     Id "LF-PlayerMenuGang_Rank"
     Target_en_us "Your rank"
     Comment "Personal file Gang tab: label of the member's own rank."
    }
    CustomStringTableItem "{03473A66F3C4EECF}" {
     Id "LF-PlayerMenuGang_SitesNone"
     Target_en_us "No sites held yet."
     Comment "Personal file Gang tab: the gang holds no site."
    }
    CustomStringTableItem "{D7E1304018F6F1B9}" {
     Id "LF-PlayerMenuGang_Waiting"
     Target_en_us "Asking your crew..."
     Comment "Personal file Gang tab: shown until the server's answer arrives."
    }
    CustomStringTableItem "{A839CFC87CD97E94}" {
     Id "LF-PlayerMenu_Bank"
     Target_en_us "Bank account"
     Comment "Personal file row: bank balance."
    }
    CustomStringTableItem "{11942C989F3F511F}" {
     Id "LF-PlayerMenu_CapClose"
     Target_en_us "Esc"
     Comment "Personal file footer: the key-cap printed before Close (the keyboard key that closes the file)."
    }
    CustomStringTableItem "{25A301633A5E7DD6}" {
     Id "LF-PlayerMenu_CapGuide"
     Target_en_us "F3"
     Comment "Personal file footer: the key-cap printed before Welcome guide (the guide key)."
    }
    CustomStringTableItem "{4F95A449245E1929}" {
     Id "LF-PlayerMenu_Cash"
     Target_en_us "Cash in hand"
     Comment "Personal file row: cash the character carries."
    }
    CustomStringTableItem "{247837F8339492C7}" {
     Id "LF-PlayerMenu_Close"
     Target_en_us "Close"
     Comment "Personal file close button."
    }
    CustomStringTableItem "{A7295323E17AA4ED}" {
     Id "LF-PlayerMenu_Days"
     Target_en_us "Island day"
     Comment "Personal file row: which day on the island the character is on (playtime counted in island days)."
    }
    CustomStringTableItem "{6F0562734D35720E}" {
     Id "LF-PlayerMenu_ErrandsDone"
     Target_en_us "Errands done"
     Comment "Personal file row label over the errand list. Value: done / on file."
    }
    CustomStringTableItem "{C42AE3E161B80F72}" {
     Id "LF-PlayerMenu_Garage"
     Target_en_us "Garage slots"
     Comment "Personal file row: vehicles stored over slots held."
    }
    CustomStringTableItem "{6FC41C4D1BFFDA61}" {
     Id "LF-PlayerMenu_GarageValue"
     Target_en_us "%1 of %2 used"
     Comment "Personal file garage value. %1 = vehicles stored, %2 = slots held."
    }
    CustomStringTableItem "{ED18878B54F73D5D}" {
     Id "LF-PlayerMenu_GroupArrival"
     Target_en_us "FIRST ERRANDS"
     Comment "Personal file Progress list group: the guided arrival chain."
    }
    CustomStringTableItem "{40343406B6555732}" {
     Id "LF-PlayerMenu_GroupCount"
     Target_en_us "%1 / %2"
     Comment "Personal file group row count. %1 = done, %2 = total."
    }
    CustomStringTableItem "{6966BAA81E2F635C}" {
     Id "LF-PlayerMenu_GroupErrands"
     Target_en_us "OTHER ERRANDS"
     Comment "Personal file Progress list group: every other errand taken."
    }
    CustomStringTableItem "{0F90061B8CB63527}" {
     Id "LF-PlayerMenu_Guide"
     Target_en_us "Welcome guide"
     Comment "Personal file footer button: opens the welcome guide."
    }
    CustomStringTableItem "{471B72A1D2FADFF6}" {
     Id "LF-PlayerMenu_HeadErrand"
     Target_en_us "THE ERRAND"
     Comment "Personal file card heading: the picked errand, read in full."
    }
    CustomStringTableItem "{F54BF1FEAB472DD9}" {
     Id "LF-PlayerMenu_HeadErrands"
     Target_en_us "ERRANDS"
     Comment "Personal file card heading: the list of errands."
    }
    CustomStringTableItem "{CF4EAC3232E8D545}" {
     Id "LF-PlayerMenu_HeadIdentity"
     Target_en_us "PERSON"
     Comment "Personal file card heading over the character preview, trade and standing."
    }
    CustomStringTableItem "{3CF77D0FEE9B529D}" {
     Id "LF-PlayerMenu_HeadLicences"
     Target_en_us "Licenses and permits"
     Comment "Personal file row label: the licences and permits held."
    }
    CustomStringTableItem "{41E30B4EE0E7DEE6}" {
     Id "LF-PlayerMenu_HeadMoney"
     Target_en_us "FINANCES"
     Comment "Personal file section heading."
    }
    CustomStringTableItem "{5EDB5A057FBC4AA3}" {
     Id "LF-PlayerMenu_HeadSkills"
     Target_en_us "SKILLS"
     Comment "Personal file section heading: one row per job skill."
    }
    CustomStringTableItem "{BA6834643BAE7C6D}" {
     Id "LF-PlayerMenu_HeadStatus"
     Target_en_us "STATUS"
     Comment "Personal file section heading."
    }
    CustomStringTableItem "{660D6443F35395AD}" {
     Id "LF-PlayerMenu_Houses"
     Target_en_us "Houses owned"
     Comment "Personal file row."
    }
    CustomStringTableItem "{16B38ECB1917D1B4}" {
     Id "LF-PlayerMenu_Identity"
     Target_en_us "%1 · %2"
     Comment "Personal file identity line. %1 = character name, %2 = role (Civilian, Police officer, on duty, ...)."
    }
    CustomStringTableItem "{134ECD8AD294067B}" {
     Id "LF-PlayerMenu_Job"
     Target_en_us "Profession"
     Comment "Personal file row: the current job of the character."
    }
    CustomStringTableItem "{5D2600F1785F6C69}" {
     Id "LF-PlayerMenu_JobNone"
     Target_en_us "No trade yet"
     Comment "Personal file row value: the character has not taken a job."
    }
    CustomStringTableItem "{82C56C8427A68BE7}" {
     Id "LF-PlayerMenu_Key"
     Target_en_us "Personal file"
     Comment "Settings > Controls: the key that opens the personal file (J)."
    }
    CustomStringTableItem "{3F1C9A7E2B5D8064}" {
     Id "LF-PlayerMenu_Ledger"
     Target_en_us "%1 ........ %2"
     Comment "Personal file ledger line, printed matter. %1 = what (Cash in hand, Houses owned), %2 = the figure."
    }
    CustomStringTableItem "{D2B263A13534625E}" {
     Id "LF-PlayerMenu_LicenceItem"
     Target_en_us "· %1"
     Comment "Personal file: one licence under the licences row. %1 = licence name."
    }
    CustomStringTableItem "{7CE8B3A156C169C1}" {
     Id "LF-PlayerMenu_Licences"
     Target_en_us "Licenses and permits: %1"
     Comment "Personal file line. %1 = the licence names, comma separated."
    }
    CustomStringTableItem "{D74A7FE19FEC6AD6}" {
     Id "LF-PlayerMenu_LicencesHeld"
     Target_en_us "%1 held"
     Comment "Personal file row value: how many licences and permits the character holds. %1 = count."
    }
    CustomStringTableItem "{88EE192FCE8AC4CC}" {
     Id "LF-PlayerMenu_LicencesNone"
     Target_en_us "Licenses and permits: none held"
     Comment "Personal file line when no licence is held."
    }
    CustomStringTableItem "{CC5DA475963D07AD}" {
     Id "LF-PlayerMenu_LicencesNoneShort"
     Target_en_us "None"
     Comment "Personal file licences row value when the character holds none."
    }
    CustomStringTableItem "{48A10D9CC6860A9F}" {
     Id "LF-PlayerMenu_NoName"
     Target_en_us "Name not on file"
     Comment "Personal file: the character has no registered name yet."
    }
    CustomStringTableItem "{CFF02279F95EF05A}" {
     Id "LF-PlayerMenu_NoQuests"
     Target_en_us "No errands on file yet."
     Comment "Personal file Progress tab with no errand listed."
    }
    CustomStringTableItem "{7377D423F2E9B5D3}" {
     Id "LF-PlayerMenu_OffDuty"
     Target_en_us "off duty"
     Comment "Personal file duty state."
    }
    CustomStringTableItem "{B6671A27755B9697}" {
     Id "LF-PlayerMenu_OnDuty"
     Target_en_us "on duty"
     Comment "Personal file duty state."
    }
    CustomStringTableItem "{4D5F797963FE8D97}" {
     Id "LF-PlayerMenu_Playtime"
     Target_en_us "Time on the island"
     Comment "Personal file row: total playtime of the account."
    }
    CustomStringTableItem "{8FB8F9587FA93741}" {
     Id "LF-PlayerMenu_PlaytimeValue"
     Target_en_us "%1 h %2 min"
     Comment "Personal file playtime value. %1 = hours, %2 = minutes."
    }
    CustomStringTableItem "{114E0805DE8D52D3}" {
     Id "LF-PlayerMenu_QuestHint"
     Target_en_us "Pick an errand to read it."
     Comment "Personal file: shown under the errands list before one is picked."
    }
    CustomStringTableItem "{08BD4D6C3BFA68E9}" {
     Id "LF-PlayerMenu_QuestProgress"
     Target_en_us "%1 · progress %2 / %3"
     Comment "Personal file errand numbers. %1 = badge (DONE, CURRENT, LATER), %2 = done, %3 = needed."
    }
    CustomStringTableItem "{E9FC44CB85111207}" {
     Id "LF-PlayerMenu_Read"
     Target_en_us "Read"
     Comment "Personal file list row button: show this skill or errand below the list."
    }
    CustomStringTableItem "{388649A98D7CA096}" {
     Id "LF-PlayerMenu_RoleCivilian"
     Target_en_us "Civilian"
     Comment "Personal file role."
    }
    CustomStringTableItem "{11AE87042B721A46}" {
     Id "LF-PlayerMenu_RoleDuty"
     Target_en_us "%1, %2"
     Comment "Personal file role with duty. %1 = role, %2 = on duty / off duty."
    }
    CustomStringTableItem "{DC5E7B83D1F8D08A}" {
     Id "LF-PlayerMenu_RoleMedic"
     Target_en_us "{Medic}"
     Comment "Personal file role."
    }
    CustomStringTableItem "{78EEA7E96D6E202A}" {
     Id "LF-PlayerMenu_RolePolice"
     Target_en_us "{Police} officer"
     Comment "Personal file role."
    }
    CustomStringTableItem "{E65F9D1445B42A9C}" {
     Id "LF-PlayerMenu_Settings"
     Target_en_us "Settings"
     Comment "Personal file footer button: opens the game settings."
    }
    CustomStringTableItem "{643BE60ED6996276}" {
     Id "LF-PlayerMenu_SkillDescOther"
     Target_en_us "Work on the island that earns experience as you do it."
     Comment "Skill description for a job the table has no row for."
    }
    CustomStringTableItem "{C852478B4B20E8CC}" {
     Id "LF-PlayerMenu_SkillDesc_CIVILIAN"
     Target_en_us "Odd jobs and errands for the locals. Some tools, a lockpick among them, want a steady hand first."
     Comment "Skill description, CIVILIAN."
    }
    CustomStringTableItem "{6108599220E495F4}" {
     Id "LF-PlayerMenu_SkillDesc_DELIVERY"
     Target_en_us "Parcel rounds, fuel runs and taxi fares. More runs open the better-paid routes."
     Comment "Skill description, DELIVERY."
    }
    CustomStringTableItem "{DDE9E42AD35085BB}" {
     Id "LF-PlayerMenu_SkillDesc_FARMER"
     Target_en_us "Picking fruit and drawing water in the fields. Experience opens the orchards and the faster harvest."
     Comment "Skill description, FARMER."
    }
    CustomStringTableItem "{16237A864952C049}" {
     Id "LF-PlayerMenu_SkillDesc_FISHER"
     Target_en_us "Rod and line off the jetty and the shore. Patience brings the bigger catch."
     Comment "Skill description, FISHER."
    }
    CustomStringTableItem "{69D82ADBBCD2826F}" {
     Id "LF-PlayerMenu_SkillDesc_LUMBERJACK"
     Target_en_us "Felling trees and working logs into planks at the mill."
     Comment "Skill description, LUMBERJACK."
    }
    CustomStringTableItem "{3BC0577878C0CAC0}" {
     Id "LF-PlayerMenu_SkillDesc_MEDIC"
     Target_en_us "Treating the hurt and bringing the fallen back, on duty as a {medic}."
     Comment "Skill description, MEDIC."
    }
    CustomStringTableItem "{DF98D27817C11D9F}" {
     Id "LF-PlayerMenu_SkillDesc_MINER"
     Target_en_us "Coal, copper, iron and gold from the pits. The richer ores want more experience first."
     Comment "Skill description, MINER."
    }
    CustomStringTableItem "{5C7860BF6542679D}" {
     Id "LF-PlayerMenu_SkillDesc_POLICE"
     Target_en_us "Patrols, stops, fines and arrests on duty with the {police}."
     Comment "Skill description, POLICE."
    }
    CustomStringTableItem "{9D5AC9F2C514EA69}" {
     Id "LF-PlayerMenu_SkillDesc_SALVAGE"
     Target_en_us "Sorting scrap and salvage into metal worth selling."
     Comment "Skill description, SALVAGE."
    }
    CustomStringTableItem "{1ED61C734E42F8A5}" {
     Id "LF-PlayerMenu_SkillDesc_SMELTING"
     Target_en_us "Furnaces, smelters, refineries and the cement works. Every finished batch earns experience here, never in Mining."
     Comment "Skill description, SMELTING."
    }
    CustomStringTableItem "{58F153B6D5B351ED}" {
     Id "LF-PlayerMenu_SkillHint"
     Target_en_us "Pick a skill to read its record."
     Comment "Personal file: shown under the skills list before one is picked."
    }
    CustomStringTableItem "{DE03E6E8DDB00C73}" {
     Id "LF-PlayerMenu_SkillLevel"
     Target_en_us "Level %1"
     Comment "Personal file skill row status. %1 = level."
    }
    CustomStringTableItem "{CDCA9B71CC6A413D}" {
     Id "LF-PlayerMenu_SkillNext"
     Target_en_us "Next level opens: %1"
     Comment "Personal file skill record: what the next level of this job unlocks. %1 = names of the work it opens, comma separated."
    }
    CustomStringTableItem "{6E14D128F51C49C6}" {
     Id "LF-PlayerMenu_SkillOpens"
     Target_en_us "Level %1 opens: %2"
     Comment "F2 skill card (#4414). %1 = the next level, %2 = what it opens."
    }
    CustomStringTableItem "{92CE57A0169506BD}" {
     Id "LF-PlayerMenu_SkillPerks"
     Target_en_us "Perks: %1"
     Comment "F2 skill card (#4414). %1 = the perks picked in this trade."
    }
    CustomStringTableItem "{710EB402141348F5}" {
     Id "LF-PlayerMenu_SkillRank"
     Target_en_us "Ranked %1 in %2 this week"
     Comment "F2 skill card weekly rank slot (#4414). %1 = the rank as an ordinal, like 6th, %2 = the trade."
    }
    CustomStringTableItem "{B0E52A4594D5C18F}" {
     Id "LF-PlayerMenu_SkillTitle"
     Target_en_us "%1, %2"
     Comment "F2 skill card title (#4414). %1 = the trade, %2 = its tier word."
    }
    CustomStringTableItem "{F52EBB28BA797478}" {
     Id "LF-PlayerMenu_SkillTop"
     Target_en_us "Level %1 · %2 XP, the top of the ladder"
     Comment "Personal file skill numbers at the highest level. %1 = level, %2 = total XP."
    }
    CustomStringTableItem "{FC2FB5AAEA09C862}" {
     Id "LF-PlayerMenu_SkillUnranked"
     Target_en_us "Not ranked yet this week"
     Comment "F2 skill card weekly rank slot (#4414) when the player is not on the shift board."
    }
    CustomStringTableItem "{48BCC2D5ADF101A4}" {
     Id "LF-PlayerMenu_SkillXp"
     Target_en_us "Level %1 · %2 / %3 XP"
     Comment "Personal file skill numbers. %1 = level, %2 = total XP, %3 = total XP of the next level."
    }
    CustomStringTableItem "{4DF107179334698F}" {
     Id "LF-PlayerMenu_Skill_CIVILIAN"
     Target_en_us "General work"
     Comment "Skill name of the CIVILIAN job track."
    }
    CustomStringTableItem "{44BD63FF4A688896}" {
     Id "LF-PlayerMenu_Skill_DELIVERY"
     Target_en_us "Delivery"
     Comment "Skill name of the DELIVERY job track (parcels, fuel, taxi)."
    }
    CustomStringTableItem "{CEB3C601BA6C13EC}" {
     Id "LF-PlayerMenu_Skill_FARMER"
     Target_en_us "Farming"
     Comment "Skill name of the FARMER job track."
    }
    CustomStringTableItem "{70C89C58806C0943}" {
     Id "LF-PlayerMenu_Skill_FISHER"
     Target_en_us "Fishing"
     Comment "Skill name of the FISHER job track."
    }
    CustomStringTableItem "{F79EF93C19B94D30}" {
     Id "LF-PlayerMenu_Skill_LUMBERJACK"
     Target_en_us "Lumber"
     Comment "Skill name of the LUMBERJACK job track."
    }
    CustomStringTableItem "{B18415A4FB204825}" {
     Id "LF-PlayerMenu_Skill_MEDIC"
     Target_en_us "{Medic}"
     Comment "Skill name of the MEDIC job track."
    }
    CustomStringTableItem "{AA52CF5828EE256B}" {
     Id "LF-PlayerMenu_Skill_MINER"
     Target_en_us "Mining"
     Comment "Skill name of the MINER job track."
    }
    CustomStringTableItem "{17460BB5F62B70AB}" {
     Id "LF-PlayerMenu_Skill_POLICE"
     Target_en_us "{Police}"
     Comment "Skill name of the POLICE job track."
    }
    CustomStringTableItem "{8D00FDBAFF6AEDED}" {
     Id "LF-PlayerMenu_Skill_SALVAGE"
     Target_en_us "Salvage"
     Comment "Skill name of the SALVAGE job track."
    }
    CustomStringTableItem "{AD86C48B222D1B97}" {
     Id "LF-PlayerMenu_Skill_SMELTING"
     Target_en_us "Smelting"
     Comment "Skill name of the SMELTING job track."
    }
    CustomStringTableItem "{D9EFF6F38B9466F2}" {
     Id "LF-PlayerMenu_StateCurrent"
     Target_en_us "CURRENT"
     Comment "Personal file errand badge."
    }
    CustomStringTableItem "{921FF4306C06E7BA}" {
     Id "LF-PlayerMenu_StateDone"
     Target_en_us "DONE"
     Comment "Personal file errand badge."
    }
    CustomStringTableItem "{6BE78DD852AA5B2B}" {
     Id "LF-PlayerMenu_StateLater"
     Target_en_us "LATER"
     Comment "Personal file errand badge."
    }
    CustomStringTableItem "{2C76F51CE39A8441}" {
     Id "LF-PlayerMenu_Subtitle"
     Target_en_us "Day %1 on the island · %2 played"
     Comment "Personal file header, right of the name. %1 = island day number, %2 = time played (3 h 20 min)."
    }
    CustomStringTableItem "{72DBFE782ACC8113}" {
     Id "LF-PlayerMenu_Summary"
     Target_en_us "Errands on file: %1 of %2 done"
     Comment "Personal file Progress tab summary. %1 = done, %2 = listed."
    }
    CustomStringTableItem "{30031590BAD269AF}" {
     Id "LF-PlayerMenu_TabFile"
     Target_en_us "My file"
     Comment "Personal file tab button: your tickets, bookings and seized licences."
    }
    CustomStringTableItem "{E6EFC108394AC7CF}" {
     Id "LF-PlayerMenu_TabGang"
     Target_en_us "Gang"
     Comment "Personal file tab button: your gang at a glance. Shown only to a gang member."
    }
    CustomStringTableItem "{564984191E6AC0B6}" {
     Id "LF-PlayerMenu_TabNext"
     Target_en_us "What to do next"
     Comment "Personal file tab: suggested next steps. Shown only when there is something to suggest."
    }
    CustomStringTableItem "{8D2E6B1F4A7C9035}" {
     Id "LF-PlayerMenu_TabOpen"
     Target_en_us "· %1 ·"
     Comment "Personal file: the open tab's button text. %1 = the tab name."
    }
    CustomStringTableItem "{8168FBFAAC62E4EB}" {
     Id "LF-PlayerMenu_TabPerson"
     Target_en_us "Person"
     Comment "Personal file tab button: name, money, status, skills."
    }
    CustomStringTableItem "{E46FF8E8480EFC07}" {
     Id "LF-PlayerMenu_TabProgress"
     Target_en_us "Progress"
     Comment "Personal file tab button: errands done, current and later."
    }
    CustomStringTableItem "{7ECEC4376AD1B4E5}" {
     Id "LF-PlayerMenu_TitlePerson"
     Target_en_us "PERSONAL FILE · PERSON"
     Comment "Personal file sheet title on the Person tab."
    }
    CustomStringTableItem "{815D3A532245CE9D}" {
     Id "LF-PlayerMenu_TitleProgress"
     Target_en_us "PERSONAL FILE · PROGRESS"
     Comment "Personal file sheet title on the Progress tab."
    }
    CustomStringTableItem "{D0846B46055E22FE}" {
     Id "LF-PlayerMenu_Waiting"
     Target_en_us "Fetching the file from the records office..."
     Comment "Personal file: shown until the server's answer arrives."
    }
    CustomStringTableItem "{E10944E60E19C2E2}" {
     Id "LF-PlayerMenu_Wanted"
     Target_en_us "Wanted"
     Comment "Personal file row: the police wanted state."
    }
    CustomStringTableItem "{0EBAA8CE0E18B34C}" {
     Id "LF-PlayerMenu_WantedLevel"
     Target_en_us "Level %1"
     Comment "Personal file wanted value. %1 = wanted level."
    }
    CustomStringTableItem "{402E66AC2C233712}" {
     Id "LF-PlayerMenu_WantedNo"
     Target_en_us "No"
     Comment "Personal file wanted value: not wanted."
    }
    CustomStringTableItem "{38FCAD58BB2A2C60}" {
     Id "LF-PlayerMenu_Waypoint"
     Target_en_us "Set waypoint to the current errand"
     Comment "Personal file button: puts the current first errand on the map and screen."
    }
    CustomStringTableItem "{F875CB040C3C2526}" {
     Id "LF-PoliceAppGm_Approve"
     Target_en_us "Approve"
     Comment "Applications sheet button."
    }
    CustomStringTableItem "{1296E6499AD624E5}" {
     Id "LF-PoliceAppGm_Approved"
     Target_en_us "APPROVED: %1"
     Comment "Applications sheet status after approving; %1 is the applicant name."
    }
    CustomStringTableItem "{0E3BC446BA9C4943}" {
     Id "LF-PoliceAppGm_BadRequest"
     Target_en_us "Request refused."
     Comment "Applications sheet status when the server refused a malformed request."
    }
    CustomStringTableItem "{1EA5C56CC7684B12}" {
     Id "LF-PoliceAppGm_Busy"
     Target_en_us "Too fast. Press again in a moment."
     Comment "Applications sheet status when the server dropped a request sent too quickly (#1288)."
    }
    CustomStringTableItem "{58CA4203A5B3554F}" {
     Id "LF-PoliceAppGm_Close"
     Target_en_us "Close"
     Comment "Applications sheet close button."
    }
    CustomStringTableItem "{15339673DC6E1BB0}" {
     Id "LF-PoliceAppGm_Count"
     Target_en_us "OPEN APPLICATIONS .... %1"
     Comment "Applications sheet status; %1 is how many are open."
    }
    CustomStringTableItem "{1DB18896201F1A7B}" {
     Id "LF-PoliceAppGm_Detail"
     Target_en_us "%1. Why: %2. Hours: %3."
     Comment "Applications sheet detail line; %1 name, %2 reason, %3 availability."
    }
    CustomStringTableItem "{8A45E10EB5B5A7DF}" {
     Id "LF-PoliceAppGm_NeedAdministrator"
     Target_en_us "Only an administrator can approve or reject. Moderators may read."
     Comment "Applications sheet status when a moderator or operator tries to decide (#1288)."
    }
    CustomStringTableItem "{95CC9997DB5D0E03}" {
     Id "LF-PoliceAppGm_PickFirst"
     Target_en_us "Read an application, then approve or reject it."
     Comment "Applications sheet detail line before one is picked."
    }
    CustomStringTableItem "{3B80676FFF309851}" {
     Id "LF-PoliceAppGm_Read"
     Target_en_us "Read"
     Comment "Applications sheet row action: show this application."
    }
    CustomStringTableItem "{343D76BAE52282A8}" {
     Id "LF-PoliceAppGm_Reject"
     Target_en_us "Reject"
     Comment "Applications sheet button."
    }
    CustomStringTableItem "{5957CB471E7B660E}" {
     Id "LF-PoliceAppGm_Rejected"
     Target_en_us "REJECTED: %1"
     Comment "Applications sheet status after rejecting; %1 is the applicant name."
    }
    CustomStringTableItem "{1EF55798C921D26C}" {
     Id "LF-PoliceAppGm_Title"
     Target_en_us "Applications to the {police}"
     Comment "Game Master applications sheet title (#1288)."
    }
    CustomStringTableItem "{7064E4B84B8AE16A}" {
     Id "LF-PoliceAppGm_Waited"
     Target_en_us "waited %1 h"
     Comment "Applications sheet row status; %1 is whole hours since filing."
    }
    CustomStringTableItem "{FC7DA9A04AC8D7ED}" {
     Id "LF-PoliceAppGm_Waiting"
     Target_en_us "Reading the file..."
     Comment "Applications sheet status while the server answers."
    }
    CustomStringTableItem "{AD999D02320AA8DB}" {
     Id "LF-PoliceApp_Action"
     Target_en_us "Apply to the police"
     Comment "Duty desk action."
    }
    CustomStringTableItem "{C1C8869AB5FF5BB9}" {
     Id "LF-PoliceApp_ActionDesc"
     Target_en_us "Fill in an application to join the police."
     Comment "Duty desk action description."
    }
    CustomStringTableItem "{3B86E7A5E27354FE}" {
     Id "LF-PoliceApp_Approved"
     Target_en_us "Application approved. Clock in at the duty desk."
     Comment "Toast when the application is approved on submit (playtest)."
    }
    CustomStringTableItem "{D85287BA39923646}" {
     Id "LF-PoliceApp_ApprovedOther"
     Target_en_us "Approved the application of %1."
     Comment "Admin toast after approving; %1 is the applicant name."
    }
    CustomStringTableItem "{E16B433C3AC6E980}" {
     Id "LF-PoliceApp_Availability"
     Target_en_us "When you can be on duty"
     Comment "Police application blank: availability."
    }
    CustomStringTableItem "{866A6764BE9188EB}" {
     Id "LF-PoliceApp_Close"
     Target_en_us "Put it away"
     Comment "Police application close button."
    }
    CustomStringTableItem "{57C1C2B309431CF5}" {
     Id "LF-PoliceApp_Error_AlreadyOpen"
     Target_en_us "You already have an application waiting."
     Comment "Refusal: one open application per player."
    }
    CustomStringTableItem "{5E8B02D6C9A17F43}" {
     Id "LF-PoliceApp_Error_Cooldown"
     Target_en_us "Your last application was rejected. You can apply again in %1."
     Comment "Refusal: the rejection cooldown (#803). %1 is the wait left, such as 2h 5m."
    }
    CustomStringTableItem "{176552BBA801A382}" {
     Id "LF-PoliceApp_Error_Fields"
     Target_en_us "Fill in all three blanks."
     Comment "Form error: a blank is empty."
    }
    CustomStringTableItem "{128D4C6358A00BA9}" {
     Id "LF-PoliceApp_Error_Length"
     Target_en_us "One of the answers is too long."
     Comment "Form error: too long."
    }
    CustomStringTableItem "{EF03D310EECF0660}" {
     Id "LF-PoliceApp_Error_NoAccount"
     Target_en_us "Register at the civil registry first."
     Comment "Refusal: no player identity."
    }
    CustomStringTableItem "{7F7AA537F0C237BB}" {
     Id "LF-PoliceApp_Error_OnRoster"
     Target_en_us "You are already on the {police} roster."
     Comment "Refusal: applicant is already police."
    }
    CustomStringTableItem "{A3D17C50B8E24F96}" {
     Id "LF-PoliceApp_Error_Playtime"
     Target_en_us "You need more play time before you can apply. Time left: %1."
     Comment "Refusal: the playtime gate (#803). %1 is the play time still missing, such as 12h 30m."
    }
    CustomStringTableItem "{8AE64C101F1D0F7D}" {
     Id "LF-PoliceApp_Intro"
     Target_en_us "Fill in all three blanks. One application per person; an administrator decides."
     Comment "Police application form instruction line."
    }
    CustomStringTableItem "{C80D8FA8411D1B2A}" {
     Id "LF-PoliceApp_Name"
     Target_en_us "Your name"
     Comment "Police application blank: name."
    }
    CustomStringTableItem "{D17A6AB7C4AE46BE}" {
     Id "LF-PoliceApp_NoneOpen"
     Target_en_us "No open applications."
     Comment "Admin review toast when none wait."
    }
    CustomStringTableItem "{B28597051ECFD842}" {
     Id "LF-PoliceApp_NotAdmin"
     Target_en_us "Only an administrator can approve applications."
     Comment "Refusal toast for a non-admin review."
    }
    CustomStringTableItem "{F39424782E4C72BE}" {
     Id "LF-PoliceApp_Receipt"
     Target_en_us "Application received. An administrator will review it."
     Comment "Receipt toast after filing."
    }
    CustomStringTableItem "{9F943D13BD5ABD5D}" {
     Id "LF-PoliceApp_Review"
     Target_en_us "Review applications (admin)"
     Comment "Admin action at the duty desk: approves the oldest open application."
    }
    CustomStringTableItem "{3EC049CF5AD21E3C}" {
     Id "LF-PoliceApp_ReviewDesc"
     Target_en_us "Approve the application that has waited longest."
     Comment "Admin review action description."
    }
    CustomStringTableItem "{62BF90390E136693}" {
     Id "LF-PoliceApp_Status"
     Target_en_us "{Police} application"
     Comment "Toast title for police application messages."
    }
    CustomStringTableItem "{E8C273DCD90D9613}" {
     Id "LF-PoliceApp_Submit"
     Target_en_us "Hand it in"
     Comment "Police application submit button."
    }
    CustomStringTableItem "{B5EC482E2E329B9F}" {
     Id "LF-PoliceApp_Title"
     Target_en_us "Application to the {Police}"
     Comment "Police application form title."
    }
    CustomStringTableItem "{71121DE17F6CC779}" {
     Id "LF-PoliceApp_Why"
     Target_en_us "Why you want to serve"
     Comment "Police application blank: reason."
    }
    CustomStringTableItem "{F5DFC610C9A4C8D8}" {
     Id "LF-PoliceCall_Bonus"
     Target_en_us "Call closed: resolved. +{currency:sym}%1 bonus, paid when you clock out."
     Comment "Toast to the officer when fining the call's suspect at the site completes their police call; %1 is the duty bonus added (#4319)"
    }
    CustomStringTableItem "{95E584A32913873F}" {
     Id "LF-PoliceCall_Paid"
     Target_en_us "Call closed: {currency:sym}%1 cash"
     Comment "Toast to the officer when fining the call's suspect at the site completes their police call; %1 is the reward"
    }
    CustomStringTableItem "{6EB531AC99FDCC19}" {
     Id "LF-PoliceCall_Title"
     Target_en_us "Call for {police}"
     Comment "Toast title for a police call event"
    }
    CustomStringTableItem "{622F0801A7A7F9E1}" {
     Id "LF-PoliceRankSet_BadRank"
     Target_en_us "There is no {police} rank %1."
     Comment "/policerank refusal: %1 is the rank number asked for (#2838)."
    }
    CustomStringTableItem "{6DBFB3950141B969}" {
     Id "LF-PoliceRankSet_Denied"
     Target_en_us "Only administrators can set {police} ranks."
     Comment "/policerank refusal: the caller is not an administrator (#2838)."
    }
    CustomStringTableItem "{21563D4ECF5D060A}" {
     Id "LF-PoliceRankSet_Done"
     Target_en_us "{police} rank set to %1."
     Comment "/policerank result: %1 is the new rank number (#2838)."
    }
    CustomStringTableItem "{357C84FBFAC0AFCC}" {
     Id "LF-PoliceRankSet_NoTarget"
     Target_en_us "No account with that id."
     Comment "/policerank refusal: the account id is unknown (#2838)."
    }
    CustomStringTableItem "{A52EC32B02107B2D}" {
     Id "LF-PoliceRankSet_Usage"
     Target_en_us "Type /policerank, an account id and a rank number."
     Comment "/policerank status when the line is not an account id and a rank number (#2838)."
    }
    CustomStringTableItem "{57D1AD3E97B72422}" {
     Id "LF-PoliceRoster_Approved"
     Target_en_us "You are on the {Police} roster: go on duty at the station locker."
     Comment "Toast at the first spawn after an application was approved while offline (#1447)."
    }
    CustomStringTableItem "{D26550CD20582ACD}" {
     Id "LF-PoliceRoster_Expired"
     Target_en_us "Your {Police} trial ran out and you are off the roster."
     Comment "Toast at the first spawn after a trial ended unpromoted (#1447)."
    }
    CustomStringTableItem "{4781500A4B2C8282}" {
     Id "LF-Police_Alert_Murder"
     Target_en_us "Crime Alert: a person was killed"
     Comment "Police notification when a player kills someone (M1-27)."
    }
    CustomStringTableItem "{6F303BE1BA9EE09E}" {
     Id "LF-Police_CallBoardTake"
     Target_en_us "Take a call"
     Comment "Action on the police dispatch board: take the oldest open call your rank may see (#2479)."
    }
    CustomStringTableItem "{1B0D73A7B04203F5}" {
     Id "LF-Police_CallNone"
     Target_en_us "No open call for your rank."
     Comment "Toast when the police board has no call this officer may take."
    }
    CustomStringTableItem "{28DC7CFE99DA35C4}" {
     Id "LF-Police_CallTaken"
     Target_en_us "Call taken: %1"
     Comment "Toast to the officer after taking a police call. %1 = the site."
    }
    CustomStringTableItem "{E07B9C887D2B46B5}" {
     Id "LF-Police_CallTitle"
     Target_en_us "Dispatch"
     Comment "Toast title for the police dispatch board."
    }
    CustomStringTableItem "{C0906C82C890BD00}" {
     Id "LF-Police_Cap_Name"
     Target_en_us "Officer's cap"
     Comment "Role uniform item name (tools/assets/police-*, doctor-coat)"
    }
    CustomStringTableItem "{7D3A91C4E85B2F60}" {
     Id "LF-Police_MenuTitle"
     Target_en_us "Station desk"
     Target_es_es "Mostrador de la comisaría"
     Comment "Title of the police station desk menu (wanted list, fines, arrests)"
    }
    CustomStringTableItem "{77F083725C2B630B}" {
     Id "LF-Police_NpcStunned"
     Target_en_us "%1 is stunned"
     Comment "Toast to the officer whose taser dart downed a town NPC (#922). %1 is the NPC's name."
    }
    CustomStringTableItem "{F06569B011E7028F}" {
     Id "LF-Police_Shirt_Name"
     Target_en_us "Officer's shirt"
     Comment "Role uniform item name (tools/assets/police-*, doctor-coat)"
    }
    CustomStringTableItem "{F75D2B3B32248836}" {
     Id "LF-Police_Stunned"
     Target_en_us "You were hit by a taser. You will get up in about %1 seconds."
     Comment "Toast to a player who was hit by a taser. %1 is the number of seconds."
    }
    CustomStringTableItem "{EC25FEE290BAEB26}" {
     Id "LF-Police_Trousers_Name"
     Target_en_us "Officer's pants"
     Comment "Role uniform item name (tools/assets/police-*, doctor-coat)"
    }
    CustomStringTableItem "{84084E0D0740DE17}" {
     Id "LF-Postal_AllDone"
     Target_en_us "Every sack here is already moved."
     Comment "Toast of the postal van run: nothing left to move here."
    }
    CustomStringTableItem "{6B1DF13F6E2D277F}" {
     Id "LF-Postal_Already"
     Target_en_us "You already have a postal run. Finish it or give it up first."
     Comment "Toast of the postal van run: one van per player."
    }
    CustomStringTableItem "{F06A8F159B8AD102}" {
     Id "LF-Postal_Cooldown"
     Target_en_us "The office has no van ready yet. Try again in a minute."
     Comment "Toast of the postal van run: a run ended a moment ago."
    }
    CustomStringTableItem "{F5F34AAAE6DA71E3}" {
     Id "LF-Postal_GiveUp"
     Target_en_us "Give up the postal run"
     Comment "Action at a post office stand: give the postal van run up, no pay."
    }
    CustomStringTableItem "{523DDFCCCF73873D}" {
     Id "LF-Postal_GivenUp"
     Target_en_us "Run given up. The van goes back to the office and nothing is paid."
     Comment "Toast of the postal van run: the player gave the run up."
    }
    CustomStringTableItem "{4B1E14F4EE7DFF88}" {
     Id "LF-Postal_HandIn"
     Target_en_us "Hand in the postal run"
     Comment "Action at the home post office: finish the postal van run and get paid."
    }
    CustomStringTableItem "{55F3AA4CDF15942D}" {
     Id "LF-Postal_Loaded"
     Target_en_us "Sack loaded (%1). When all are in, drive to the post office marked on your map."
     Comment "Toast of the postal van run: one sack loaded. %1 = sacks loaded/needed."
    }
    CustomStringTableItem "{018A555EA19D0BE2}" {
     Id "LF-Postal_Lost_abandoned"
     Target_en_us "You left the post van too long. The run is off and nothing is paid."
     Comment "Toast of the postal van run: the run ended because the driver abandoned the van."
    }
    CustomStringTableItem "{0573B052DDDC99B2}" {
     Id "LF-Postal_Lost_late"
     Target_en_us "The postal run took too long. The van went back and nothing is paid."
     Comment "Toast of the postal van run: the run ended past its time limit."
    }
    CustomStringTableItem "{D3E4B1F64D940F43}" {
     Id "LF-Postal_Lost_van"
     Target_en_us "The post van was wrecked or lost. The run is off and nothing is paid."
     Comment "Toast of the postal van run: the run ended because the van was destroyed."
    }
    CustomStringTableItem "{61FA7D09A2DDB08F}" {
     Id "LF-Postal_NoLicence"
     Target_en_us "You need a driver's license to drive the post van."
     Comment "Toast of the postal van run: refused, no driving licence."
    }
    CustomStringTableItem "{77EAF4A73900DC15}" {
     Id "LF-Postal_NoOffice"
     Target_en_us "There is no other post office far enough away to send mail to."
     Comment "Toast of the postal van run: the map has no destination office."
    }
    CustomStringTableItem "{1913FC9C39B45520}" {
     Id "LF-Postal_NoRun"
     Target_en_us "You have no postal run open."
     Comment "Toast of the postal van run: a step with no run."
    }
    CustomStringTableItem "{BE0D61A0250B0B0D}" {
     Id "LF-Postal_NoSpot"
     Target_en_us "There is no clear space to bring the van round. Try again shortly."
     Comment "Toast of the postal van run: no clear spot for the van."
    }
    CustomStringTableItem "{82D1E71BB544C4CC}" {
     Id "LF-Postal_NotLoaded"
     Target_en_us "Some mail sacks are still to move."
     Comment "Toast of the postal van run: a sack is still to load or unload."
    }
    CustomStringTableItem "{238A53B626C53408}" {
     Id "LF-Postal_Paid"
     Target_en_us "Postal run done. You were paid {currency:sym}%1."
     Comment "Toast of the postal van run: the run paid. %1 = the cash."
    }
    CustomStringTableItem "{837C9153DC3D1FAA}" {
     Id "LF-Postal_Sack"
     Target_en_us "Load or unload a mail sack"
     Comment "Action at a post office stand: move one mail sack into or out of the post van."
    }
    CustomStringTableItem "{96B1622BBCAD89C3}" {
     Id "LF-Postal_Start"
     Target_en_us "Take the postal van run"
     Comment "Action at a post office stand: start the postal van run."
    }
    CustomStringTableItem "{3C66967567AE5B29}" {
     Id "LF-Postal_StartDesc"
     Target_en_us "Borrow the post van, carry the mail sacks to the next post office and bring the van back. A run handed in too fast pays nothing."
     Comment "Description of the action that starts the postal van run."
    }
    CustomStringTableItem "{5B2665B387B8B998}" {
     Id "LF-Postal_Started"
     Target_en_us "The post van is parked by the office and its key is in your pocket. Load the mail sacks into it here. Hand the run in after at least %1 minutes, or nothing is paid."
     Comment "Toast of the postal van run: the run began."
    }
    CustomStringTableItem "{108D5BC1F68C84DF}" {
     Id "LF-Postal_Title"
     Target_en_us "Postal run"
     Comment "Title of the postal van run's toasts."
    }
    CustomStringTableItem "{1A0210D5BCC4C762}" {
     Id "LF-Postal_TooFast"
     Target_en_us "Too fast: a postal run pays only after %1 minutes on the road. Nothing is paid yet. Hand it in again later."
     Comment "Toast of the postal van run: handed in faster than the run's least time."
    }
    CustomStringTableItem "{EA1D8BDBDABE7D01}" {
     Id "LF-Postal_Unloaded"
     Target_en_us "Sack unloaded (%1). When all are out, drive the van back and hand in the run."
     Comment "Toast of the postal van run: one sack unloaded. %1 = sacks unloaded/needed."
    }
    CustomStringTableItem "{D73DA4B641DC454E}" {
     Id "LF-Postal_VanFar"
     Target_en_us "Bring the van closer to the office first."
     Comment "Toast of the postal van run: the van is not at the office."
    }
    CustomStringTableItem "{CBCD8E065CD8DCBC}" {
     Id "LF-Postal_Warn"
     Target_en_us "Get back to the post van within %1 seconds or the run is called off and nothing is paid."
     Comment "Toast to a postal driver who has walked away from the van, before the run is called off."
    }
    CustomStringTableItem "{3243A56EDB316D6E}" {
     Id "LF-Postal_WrongOffice"
     Target_en_us "This is not where this run's mail goes."
     Comment "Toast of the postal van run: wrong office for this step."
    }
    CustomStringTableItem "{3A7C91E05B2D4F68}" {
     Id "LF-Process_Busy"
     Target_en_us "Still running. Wait for it to finish."
     Comment "Processing refused (#3229): a timed machine already has a run under way."
    }
    CustomStringTableItem "{52D02C80CEB2B855}" {
     Id "LF-Process_FeedFromTruck"
     Target_en_us "Feed from the truck"
     Comment "Action at a processing station (#4191): take the inputs from the van or truck backed up to it."
    }
    CustomStringTableItem "{4140D26F74D7F5F3}" {
     Id "LF-Process_Needs"
     Target_en_us "Needs %1"
     Comment "Processing refused: the player lacks one input of the recipe (#3226). %1 = item name, e.g. Coal"
    }
    CustomStringTableItem "{60B6DD582597F066}" {
     Id "LF-Process_NoItems"
     Target_en_us "Missing the input materials"
     Comment "Processing refused: the player does not carry the input items"
    }
    CustomStringTableItem "{0A0EEB9828847F4A}" {
     Id "LF-Process_RunningLeft"
     Target_en_us "%1: %2 left"
     Comment "Action row on a timed machine while it runs (first-2h review B10). %1 = the machine action (Smelt iron nuggets), %2 = time left as m:ss."
    }
    CustomStringTableItem "{9D4E21B7C0A35F86}" {
     Id "LF-Process_Started"
     Target_en_us "Running. The finished goods come out here in %1 seconds; the time left shows on the machine, so get on with something else meanwhile."
     Comment "Toast (#3229): a timed machine took the inputs. %1 = seconds the run takes."
    }
    CustomStringTableItem "{11D3E9515907FBC7}" {
     Id "LF-Process_TruckLocked"
     Target_en_us "The truck is locked"
     Comment "Feed from the truck refused (#4191): the vehicle backed up to the station is locked."
    }
    CustomStringTableItem "{C1E9F68FC02150FF}" {
     Id "LF-Property_Bought"
     Target_en_us "Plot %1 is yours."
     Comment "Toast: the plot purchase went through (#2649). %1 = the plot."
    }
    CustomStringTableItem "{6B1E0D4C9A2F7355}" {
     Id "LF-Property_BuildItemsFirst"
     Target_en_us "Bring the materials first."
     Comment "Build site refusal (#2650): work asked before the stage's items are in."
    }
    CustomStringTableItem "{6B1E0D4C9A2F7351}" {
     Id "LF-Property_BuildNeed"
     Target_en_us "Need %1 more %2"
     Comment "Deposit action at a build site (#2650). %1 = how many the stage still lacks, %2 = the item's name."
    }
    CustomStringTableItem "{6B1E0D4C9A2F7353}" {
     Id "LF-Property_BuildNoItems"
     Target_en_us "You carry none of what this stage still needs."
     Comment "Build site refusal (#2650): nothing to deposit."
    }
    CustomStringTableItem "{6B1E0D4C9A2F7354}" {
     Id "LF-Property_BuildNotYours"
     Target_en_us "Only the owner and key holders can build here."
     Comment "Build site refusal (#2650, #2815): the caller neither owns the plot nor carries its key."
    }
    CustomStringTableItem "{6B1E0D4C9A2F7352}" {
     Id "LF-Property_BuildWork"
     Target_en_us "Build (%1/%2)"
     Comment "Held work action at a build site (#2650). %1 = work done, %2 = work the stage needs."
    }
    CustomStringTableItem "{55D1BC2D7557DA0C}" {
     Id "LF-Property_Buy"
     Target_en_us "Buy plot (%1)"
     Comment "Action on a for-sale plot sign (#2649). %1 = the formatted price (LF_MoneyFormat)."
    }
    CustomStringTableItem "{4263DB02B36FDA37}" {
     Id "LF-Property_HouseBought"
     Target_en_us "You bought the house. The key is in your inventory."
     Comment "Toast: the house purchase went through (#3207). %1 = the house id."
    }
    CustomStringTableItem "{D44434DE82747CDF}" {
     Id "LF-Property_HouseBuy"
     Target_en_us "Buy house (%1, %2)"
     Comment "Action on a house for-sale sign (#3207). %1 = the tier label, %2 = the formatted price (LF_MoneyFormat)."
    }
    CustomStringTableItem "{E8139375F9985FE0}" {
     Id "LF-Property_HouseNoCash"
     Target_en_us "You do not carry enough cash for this house. Take the price out of the bank at an ATM, then try the sign again."
     Comment "Toast: house purchase refused, short of cash; nothing was taken (#3207)."
    }
    CustomStringTableItem "{32F7CFE0FED0514B}" {
     Id "LF-Property_HouseNotHere"
     Target_en_us "There is no house for sale here."
     Comment "Toast: house purchase refused, no for-sale house sign in reach (#3207)."
    }
    CustomStringTableItem "{1DD4EE13D4F89E6A}" {
     Id "LF-Property_HouseOwned"
     Target_en_us "This house is already sold."
     Comment "Toast: house purchase refused, somebody owns it (#3207)."
    }
    CustomStringTableItem "{913DC2A62580BE71}" {
     Id "LF-Property_HouseSold"
     Target_en_us "Sold"
     Comment "Action label on a house sign once somebody owns the house (#3207); it cannot be used."
    }
    CustomStringTableItem "{2E4ED8FC8172E6AA}" {
     Id "LF-Property_NoCash"
     Target_en_us "You do not carry enough cash for this plot."
     Comment "Toast: plot purchase refused, short of cash; nothing was taken (#2649)."
    }
    CustomStringTableItem "{5CC34CD87EB4192B}" {
     Id "LF-Property_NotHere"
     Target_en_us "There is no plot for sale here."
     Comment "Toast: plot purchase refused, no for-sale plot sign in reach (#2649)."
    }
    CustomStringTableItem "{7A2A3649718F2743}" {
     Id "LF-Property_Owned"
     Target_en_us "This plot is already owned."
     Comment "Toast: plot purchase refused, somebody owns it (#2649)."
    }
    CustomStringTableItem "{A3C2CA7CE0677234}" {
     Id "LF-Property_SetHome"
     Target_en_us "Make this my spawn"
     Comment "House door action: the owner makes this house where they wake up when they log in"
    }
    CustomStringTableItem "{92136B0C3106B107}" {
     Id "LF-Property_Title"
     Target_en_us "Property"
     Comment "Toast title for a plot purchase answer (#2649)."
    }
    CustomStringTableItem "{3427A1D0C9E84B17}" {
     Id "LF-Qual_DetectiveWork"
     Target_en_us "That's detective work. The training desk runs the course."
     Comment "Toast to an officer without the detective qualification who tried the full vehicle search or to open a case on the case board (#3427)."
    }
    CustomStringTableItem "{2E48875B943A483E}" {
     Id "LF-Qual_Held"
     Target_en_us "You already hold this qualification."
     Comment "Qualifications: the course is already held."
    }
    CustomStringTableItem "{8BF9C67BE39BDC9F}" {
     Id "LF-Qual_RankTooLow"
     Target_en_us "You need rank %1 to take this course."
     Comment "Qualifications: %1 is the minimum rank."
    }
    CustomStringTableItem "{3F57A312C35253F9}" {
     Id "LF-Qual_ShortPurse"
     Target_en_us "You need %1 in cash to take this course."
     Comment "Qualifications: %1 is the fee."
    }
    CustomStringTableItem "{4FC9952C71D9E1F0}" {
     Id "LF-Qual_Unknown"
     Target_en_us "There is no such course."
     Comment "Qualifications: the course id is not in the table."
    }
    CustomStringTableItem "{EB5A73F654E37D13}" {
     Id "LF-Qual_WrongFaction"
     Target_en_us "This course is not open to your service."
     Comment "Qualifications: the character is of the wrong faction."
    }
   CustomStringTableItem "{468EF5CBF215416A}" {
    Id "LF-Quest_Accept"
    Target_en_us "Accept"
    Target_de_de "Annehmen"
    Target_fr_fr "Accepter"
    Target_it_it "Accetta"
    Target_es_es "Aceptar"
    Target_pt_pt "Aceitar"
    Target_pt_br "Aceitar"
    Comment "Quest accept button label"
   }
   CustomStringTableItem "{C12D7EFE7EE35379}" {
    Id "LF-Quest_Accepted"
    Target_en_us "Quest accepted"
    Target_de_de "Auftrag angenommen"
    Target_fr_fr "Quête acceptée"
    Target_it_it "Missione accettata"
    Target_es_es "Misión aceptada"
    Target_pt_pt "Missão aceite"
    Target_pt_br "Missão aceita"
    Comment "Quest accepted notification"
   }
   CustomStringTableItem "{E2E355A0237019E3}" {
    Id "LF-Quest_Action"
    Target_en_us "Talk to Quest Giver"
    Target_de_de "Mit Questgeber sprechen"
    Target_fr_fr "Parler au donneur de quêtes"
    Target_it_it "Parla con il mercante di missioni"
    Target_es_es "Hablar con el dador de misiones"
    Target_pt_pt "Falar com o doador de missões"
    Target_pt_br "Falar com o doador de missões"
    Comment "Quest giver interaction action name"
   }
    CustomStringTableItem "{DD20CCA577AB2B68}" {
     Id "LF-Quest_Action_Clerk"
     Target_en_us "Talk to the clerk"
     Comment "Interaction on the town clerk NPC."
    }
    CustomStringTableItem "{3768F00000000011}" {
     Id "LF-Quest_Action_PoliceDesk"
     Target_en_us "Speak to the desk"
     Comment "Action on the duty desk: talk about a report or hand something in (#3768)."
    }
    CustomStringTableItem "{D85E266C2ACFBDC1}" {
     Id "LF-Quest_ArrivalBank_Desc"
     Target_en_us "Paid in cash. Cash in a pocket is lost if you die, money in the bank is not. Deposit some at an ATM."
     Comment "Arrival chain step 4: deposit any amount at the ATM."
    }
    CustomStringTableItem "{94FCA4937AA67918}" {
     Id "LF-Quest_ArrivalBank_Title"
     Target_en_us "Bank it"
     Comment "Arrival chain step 4 title."
    }
    CustomStringTableItem "{B58FFAF3FDBE501F}" {
     Id "LF-Quest_ArrivalBoard_Desc"
     Target_en_us "Read the town board: it shows where everything in town is."
     Comment "Arrival chain step 2: shown when the clerk hands the card over. Read the town map sign."
    }
    CustomStringTableItem "{D6DBC19947C15B40}" {
     Id "LF-Quest_ArrivalBoard_Title"
     Target_en_us "The town board"
     Comment "Arrival chain step 2 title."
    }
    CustomStringTableItem "{FACD3C7B2CC8836C}" {
     Id "LF-Quest_ArrivalDone_Desc"
     Target_en_us "Paid and banked. Go back to the registry clerk."
     Comment "Arrival chain step 5: return to the clerk."
    }
    CustomStringTableItem "{69E9027CB344A15B}" {
     Id "LF-Quest_ArrivalDone_Title"
     Target_en_us "Settled in"
     Comment "Arrival chain step 5 title."
    }
    CustomStringTableItem "{B01582B10F8D629D}" {
     Id "LF-Quest_ArrivalDone_TurnIn"
     Target_en_us "Welcome, here is something to start with. The town is yours now: the shops, the car lot, the police station and the organization office are all on the town board."
     Comment "The clerk at the end of the arrival chain, with the welcome cash."
    }
    CustomStringTableItem "{8D29DF46F50BC183}" {
     Id "LF-Quest_ArrivalRegister_Desc"
     Target_en_us "You are new in {town} and carry no papers. The registry clerk keeps the register: talk to them and they hand you your ID card."
     Comment "Arrival chain step 1: talk to the clerk, receive the ID card."
    }
    CustomStringTableItem "{002662BFCD8D20B3}" {
     Id "LF-Quest_ArrivalRegister_Title"
     Target_en_us "New in town"
     Comment "Arrival chain step 1 title."
    }
    CustomStringTableItem "{419B451194328DF6}" {
     Id "LF-Quest_ArrivalRegister_TurnIn"
     Target_en_us "The clerk writes your name in the register and hands you your ID card. Keep it on you: the police may ask to see it."
     Comment "Arrival chain step 1: the clerk hands over the ID card."
    }
    CustomStringTableItem "{172E338F853864B0}" {
     Id "LF-Quest_ArrivalWork_Desc"
     Target_en_us "Now earn your first wages. Take courier work at the post office counter, pick up the parcel, carry it to the drop and complete the run. The post office sign tells you more."
     Comment "Arrival chain step 3: sign up at the post office and deliver one parcel."
    }
    CustomStringTableItem "{E9005E06D70F9493}" {
     Id "LF-Quest_ArrivalWork_Title"
     Target_en_us "First wages"
     Comment "Arrival chain step 3 title."
    }
    CustomStringTableItem "{1A709CDB196EBDDA}" {
     Id "LF-Quest_Clerk_Idle"
     Target_en_us "You are settled in. The town board on the square shows the rest: shops, the car lot, the police station, the organization office."
     Comment "The town clerk after the arrival chain, when there is nothing left to hand out."
    }
   CustomStringTableItem "{DA61010D2314BF43}" {
    Id "LF-Quest_Expired"
    Target_en_us "Quest expired"
    Target_de_de "Auftrag abgelaufen"
    Target_fr_fr "Quête expirée"
    Target_it_it "Missione scaduta"
    Target_es_es "Misión expirada"
    Target_pt_pt "Missão expirada"
    Target_pt_br "Missão expirada"
    Comment "Timed quest expired notification"
   }
    CustomStringTableItem "{9823FC0642192065}" {
     Id "LF-Quest_Giver_Clerk"
     Target_en_us "Town clerk"
     Comment "Quest menu title for the town clerk on the square (arrival chain)."
    }
    CustomStringTableItem "{3768F00000000012}" {
     Id "LF-Quest_Giver_PoliceDesk"
     Target_en_us "Duty desk"
     Comment "Name of the duty desk as a quest giver (#3768)."
    }
    CustomStringTableItem "{028FE3699291F64C}" {
     Id "LF-Quest_Giver_Start"
     Target_en_us "Town hall clerk"
     Target_es_es "Funcionario del ayuntamiento"
     Comment "Quest menu title for the starting quest giver"
    }
    CustomStringTableItem "{945AA58155A88BAC}" {
     Id "LF-Quest_Giver_Tester"
     Target_en_us "Test quest board"
     Comment "Quest menu title for the test quest giver in the test annex."
    }
   CustomStringTableItem "{6BA7F1501B064DDB}" {
    Id "LF-Quest_InProgress"
    Target_en_us "In progress"
    Target_de_de "In Arbeit"
    Target_fr_fr "En cours"
    Target_it_it "In corso"
    Target_es_es "En curso"
    Target_pt_pt "Em curso"
    Target_pt_br "Em andamento"
    Comment "Active quest button placeholder"
   }
   CustomStringTableItem "{2E57F8F9E638EE91}" {
    Id "LF-Quest_IntroArea_Desc"
    Target_en_us "Visit the marked area near the start point."
    Target_de_de "Besuche das markierte Gebiet nahe dem Startpunkt."
    Target_fr_fr "Visitez la zone marquée près du point de départ."
    Target_it_it "Visita l'area segnata vicino al punto di partenza."
    Target_es_es "Visita la zona marcada cerca del punto de inicio."
    Target_pt_pt "Visite a área marcada perto do ponto de partida."
    Target_pt_br "Visite a área marcada perto do ponto de partida."
    Comment "Area quest description"
   }
   CustomStringTableItem "{C77DB039F02C2043}" {
    Id "LF-Quest_IntroArea_Title"
    Target_en_us "Know the Area"
    Target_de_de "Die Gegend kennenlernen"
    Target_fr_fr "Connaître le secteur"
    Target_it_it "Conosci la zona"
    Target_es_es "Conoce la zona"
    Target_pt_pt "Conheça a área"
    Target_pt_br "Conheça a área"
    Comment "Area quest title"
   }
   CustomStringTableItem "{EE99865B91651AE5}" {
    Id "LF-Quest_IntroCollect_Desc"
    Target_en_us "Bring a water bottle to the quest giver."
    Target_de_de "Bring eine Wasserflasche zum Questgeber."
    Target_fr_fr "Apportez une bouteille d'eau au donneur de quêtes."
    Target_it_it "Porta una bottiglia d'acqua al mercante di missioni."
    Target_es_es "Lleva una botella de agua al dador de misiones."
    Target_pt_pt "Leve uma garrafa de água ao doador de missões."
    Target_pt_br "Leve uma garrafa de água ao doador de missões."
    Comment "Collect quest description"
   }
   CustomStringTableItem "{C9634CE0587CFA70}" {
    Id "LF-Quest_IntroCollect_Title"
    Target_en_us "Drinks on Me"
    Target_de_de "Getränke gefällig"
    Target_fr_fr "Une petite soif"
    Target_it_it "Un giro di bevute"
    Target_es_es "Invita a la casa"
    Target_pt_pt "Uma ronda de bebidas"
    Target_pt_br "Uma rodada de bebidas"
    Comment "Collect quest title"
   }
   CustomStringTableItem "{A38C998BD1D37CDD}" {
    Id "LF-Quest_IntroGather_Desc"
    Target_en_us "Gather 3 items in the world."
    Target_de_de "Sammle 3 Gegenstände in der Welt."
    Target_fr_fr "Récoltez 3 objets dans le monde."
    Target_it_it "Raccogli 3 oggetti nel mondo."
    Target_es_es "Recoge 3 objetos en el mundo."
    Target_pt_pt "Colete 3 itens no mundo."
    Target_pt_br "Colete 3 itens no mundo."
    Comment "Gather quest description"
   }
   CustomStringTableItem "{CEE0F1CF5CC3AD51}" {
    Id "LF-Quest_IntroGather_Title"
    Target_en_us "Gatherer's Start"
    Target_de_de "Sammler-Einstieg"
    Target_fr_fr "Première récolte"
    Target_it_it "Inizio del raccoglitore"
    Target_es_es "Inicio del recolector"
    Target_pt_pt "Início do coletor"
    Target_pt_br "Início do coletor"
    Comment "Gather quest title"
   }
   CustomStringTableItem "{64A90BC62007EB6E}" {
    Id "LF-Quest_IntroKill_Desc"
    Target_en_us "Eliminate 2 enemy players."
    Target_de_de "Schalte 2 feindliche Spieler aus."
    Target_fr_fr "Éliminez 2 joueurs ennemis."
    Target_it_it "Elimina 2 giocatori nemici."
    Target_es_es "Elimina a 2 jugadores enemigos."
    Target_pt_pt "Elimine 2 jogadores inimigos."
    Target_pt_br "Elimine 2 jogadores inimigos."
    Comment "Kill quest description"
   }
   CustomStringTableItem "{ECACEA89B00C7695}" {
    Id "LF-Quest_IntroKill_Title"
    Target_en_us "Show of Force"
    Target_de_de "Zeichen der Stärke"
    Target_fr_fr "Montrer sa force"
    Target_it_it "Mostra di forza"
    Target_es_es "Demostración de fuerza"
    Target_pt_pt "Demonstração de força"
    Target_pt_br "Demonstração de força"
    Comment "Kill quest title"
   }
   CustomStringTableItem "{D40D1DC625C5FAA9}" {
    Id "LF-Quest_IntroTalk_Desc"
    Target_en_us "Talk to the quest giver to learn how quests work."
    Target_de_de "Sprich mit dem Questgeber, um zu lernen, wie Aufträge funktionieren."
    Target_fr_fr "Parlez au donneur de quêtes pour apprendre leur fonctionnement."
    Target_it_it "Parla con il mercante di missioni per imparare come funzionano."
    Target_es_es "Habla con el dador de misiones para aprender cómo funcionan."
    Target_pt_pt "Fale com o doador de missões para aprender como funcionam."
    Target_pt_br "Fale com o doador de missões para aprender como funcionam."
    Comment "Onboarding quest description"
   }
   CustomStringTableItem "{8E5F1CF2509E3CC0}" {
    Id "LF-Quest_IntroTalk_Title"
    Target_en_us "First Steps"
    Target_de_de "Erste Schritte"
    Target_fr_fr "Premiers pas"
    Target_it_it "Primi passi"
    Target_es_es "Primeros pasos"
    Target_pt_pt "Primeiros passos"
    Target_pt_br "Primeiros passos"
    Comment "Onboarding quest title"
   }
    CustomStringTableItem "{1987FE8FB0973B18}" {
     Id "LF-Quest_IntroTimed_Desc"
     Target_en_us "Deliver 2 water bottles within 2 minutes."
    Target_de_de "Liefere 2 Wasserflaschen innerhalb von 2 Minuten."
    Target_fr_fr "Livrez 2 bouteilles d'eau en 2 minutes."
    Target_it_it "Consegna 2 bottiglie d'acqua entro 2 minuti."
    Target_es_es "Entrega 2 botellas de agua en 2 minutos."
    Target_pt_pt "Entregue 2 garrafas de água em 2 minutos."
     Target_pt_br "Entregue 2 garrafas de água em 2 minutos."
     Comment "Timed quest description"
    }
   CustomStringTableItem "{C39AC0E1DD9064FD}" {
    Id "LF-Quest_IntroTimed_Title"
    Target_en_us "Quick Delivery"
    Target_de_de "Schnelle Lieferung"
    Target_fr_fr "Livraison express"
    Target_it_it "Consegna rapida"
    Target_es_es "Entrega rápida"
    Target_pt_pt "Entrega rápida"
    Target_pt_br "Entrega rápida"
    Comment "Timed quest title"
   }
   CustomStringTableItem "{9A063765A5B8123E}" {
    Id "LF-Quest_MissingItems"
    Target_en_us "You no longer hold the required items"
    Target_de_de "Du hast die benötigten Gegenstände nicht mehr"
    Target_fr_fr "Vous n'avez plus les objets requis"
    Target_it_it "Non hai più gli oggetti richiesti"
    Target_es_es "Ya no tienes los objetos requeridos"
    Target_pt_pt "Já não tem os itens necessários"
    Target_pt_br "Você não tem mais os itens necessários"
    Comment "Turn-in refused after collect recount"
   }
   CustomStringTableItem "{F6AB60C375531892}" {
    Id "LF-Quest_NotComplete"
    Target_en_us "Quest not complete yet"
    Target_de_de "Auftrag noch nicht abgeschlossen"
    Target_fr_fr "Quête pas encore terminée"
    Target_it_it "Missione non ancora completata"
    Target_es_es "Misión aún no completada"
    Target_pt_pt "Missão ainda não concluída"
    Target_pt_br "Missão ainda não concluída"
    Comment "Turn-in refused, quest not complete"
   }
    CustomStringTableItem "{3768F00000000013}" {
     Id "LF-Quest_PoliceDesk_Idle"
     Target_en_us "Nothing for the desk right now."
     Comment "The duty desk when there is nothing to hand in or report (#3768)."
    }
   CustomStringTableItem "{C62FE3743E00B28C}" {
    Id "LF-Quest_Rejected"
    Target_en_us "Quest not available"
    Target_de_de "Auftrag nicht verfügbar"
    Target_fr_fr "Quête non disponible"
    Target_it_it "Missione non disponibile"
    Target_es_es "Misión no disponible"
    Target_pt_pt "Missão não disponível"
    Target_pt_br "Missão não disponível"
    Comment "Quest accept rejected notification"
   }
   CustomStringTableItem "{58FE2B1ECE005A69}" {
    Id "LF-Quest_Status_Available"
    Target_en_us "Available"
    Target_de_de "Verfügbar"
    Target_fr_fr "Disponible"
    Target_it_it "Disponibile"
    Target_es_es "Disponible"
    Target_pt_pt "Disponível"
    Target_pt_br "Disponível"
    Comment "Quest status: available"
   }
   CustomStringTableItem "{C07FEC9957C93531}" {
    Id "LF-Quest_Status_Complete"
    Target_en_us "Complete"
    Target_de_de "Abgeschlossen"
    Target_fr_fr "Terminée"
    Target_it_it "Completata"
    Target_es_es "Completada"
    Target_pt_pt "Concluída"
    Target_pt_br "Concluída"
    Comment "Quest status: complete, ready to turn in"
   }
   CustomStringTableItem "{943A6AA99E7319EE}" {
    Id "LF-Quest_Status_Failed"
    Target_en_us "Failed"
    Target_de_de "Fehlgeschlagen"
    Target_fr_fr "Échouée"
    Target_it_it "Fallita"
    Target_es_es "Fallida"
    Target_pt_pt "Falhada"
    Target_pt_br "Falhada"
    Comment "Quest status: timed out"
   }
   CustomStringTableItem "{CCB84CC4FAA1DA07}" {
    Id "LF-Quest_Status_TurnedIn"
    Target_en_us "Turned in"
    Target_de_de "Abgegeben"
    Target_fr_fr "Rendue"
    Target_it_it "Consegnata"
    Target_es_es "Entregada"
    Target_pt_pt "Entregue"
    Target_pt_br "Entregue"
    Comment "Quest status: reward claimed"
   }
   CustomStringTableItem "{8433759EB8ED5E85}" {
    Id "LF-Quest_Title"
    Target_en_us "Quests"
    Target_de_de "Aufträge"
    Target_fr_fr "Missions"
    Target_it_it "Missioni"
    Target_es_es "Misiones"
    Target_pt_pt "Missões"
    Target_pt_br "Missões"
    Comment "Quest menu title"
   }
   CustomStringTableItem "{4D6C52F92E7C8237}" {
    Id "LF-Quest_TurnIn"
    Target_en_us "Turn In"
    Target_de_de "Abgeben"
    Target_fr_fr "Rendre"
    Target_it_it "Consegna"
    Target_es_es "Entregar"
    Target_pt_pt "Entregar"
    Target_pt_br "Entregar"
    Comment "Quest turn-in button label"
   }
   CustomStringTableItem "{D70E71F61CD5B47F}" {
    Id "LF-Quest_TurnedIn"
    Target_en_us "Quest completed - reward granted"
    Target_de_de "Auftrag abgeschlossen - Belohnung erhalten"
    Target_fr_fr "Quête terminée - récompense accordée"
    Target_it_it "Missione completata - ricompensa ottenuta"
    Target_es_es "Misión completada - recompensa otorgada"
    Target_pt_pt "Missão concluída - recompensa concedida"
    Target_pt_br "Missão concluída - recompensa concedida"
    Comment "Quest turned in notification"
   }
    CustomStringTableItem "{F9D7252954C6A302}" {
     Id "LF-Quiz_Attempts"
     Target_en_us "Wrong attempts left before you are asked to leave: %1"
     Comment "Line under a rules quiz question; %1 = attempts left (#915)."
    }
    CustomStringTableItem "{5E293950BE2CDF2B}" {
     Id "LF-Quiz_Checking"
     Target_en_us "The clerk is checking your answers."
     Comment "Shown while the server scores the rules quiz (#915)."
    }
    CustomStringTableItem "{BBC2C86D1E6F9C34}" {
     Id "LF-Quiz_Continue"
     Target_en_us "Continue to registration"
     Comment "Button on the rules quiz pass page (#915)."
    }
    CustomStringTableItem "{8D0E5B1548ABDA78}" {
     Id "LF-Quiz_Failed"
     Target_en_us "You answered %1 of %2 correctly. %3 are needed. Read the rules below and try again."
     Comment "Rules quiz fail page; %1 = right, %2 = questions, %3 = needed (#915)."
    }
    CustomStringTableItem "{2F605F4A96D729D6}" {
     Id "LF-Quiz_Next"
     Target_en_us "Next question"
     Comment "Button on the rules quiz feedback page."
    }
    CustomStringTableItem "{0D01FDAE07097973}" {
     Id "LF-Quiz_NoAttempts"
     Target_en_us "That was your last attempt. Read the server rules and join again."
     Comment "Rules quiz fail page when the attempts are used up and the player is about to be removed (#915)."
    }
    CustomStringTableItem "{4F36E5ABF037B2D0}" {
     Id "LF-Quiz_Passed"
     Target_en_us "You answered %1 of %2 correctly. Thank you. Please go on to registration."
     Comment "Rules quiz pass page; %1 = right, %2 = questions (#915)."
    }
    CustomStringTableItem "{DD98664E993E37C4}" {
     Id "LF-Quiz_Progress"
     Target_en_us "Question %1 of %2"
     Comment "Heading above each rules quiz question; %1 = number, %2 = how many (#915)."
    }
    CustomStringTableItem "{6463B2A22B72FF57}" {
     Id "LF-Quiz_Retry"
     Target_en_us "Try again"
     Comment "Button on the rules quiz fail page (#915)."
    }
    CustomStringTableItem "{34A7F3239AB0E500}" {
     Id "LF-Quiz_Right"
     Target_en_us "Right."
     Comment "Heading on the rules quiz feedback page after a correct answer."
    }
    CustomStringTableItem "{17745EEB536A35A9}" {
     Id "LF-Quiz_Rules"
     Target_en_us "THE RULES YOU MISSED"
     Comment "Heading over the explanations on the rules quiz fail page (#915)."
    }
    CustomStringTableItem "{3599C8D083954456}" {
     Id "LF-Quiz_SeeResult"
     Target_en_us "See my result"
     Comment "Button on the rules quiz feedback page after the last question."
    }
    CustomStringTableItem "{1682AE4AD3653A8D}" {
     Id "LF-Quiz_Title"
     Target_en_us "RULES ORIENTATION"
     Comment "Printed heading of the rules quiz sheet (#915)."
    }
    CustomStringTableItem "{5CD3CA45AE1C3A5D}" {
     Id "LF-Quiz_Wrong"
     Target_en_us "Not quite. This is the rule:"
     Comment "Heading on the rules quiz feedback page after a wrong answer; the rule line follows."
    }
    CustomStringTableItem "{6DCFAEB06DBB55B8}" {
     Id "LF-RaceBoard_None"
     Target_en_us "No times yet. Win a race to set one."
     Comment "Race record board line when no course has a record (#1925)."
    }
    CustomStringTableItem "{75DCC86B3E4169B4}" {
     Id "LF-RaceBoard_Read"
     Target_en_us "Read the record board"
     Comment "Action on the board at the race track (#1925)."
    }
    CustomStringTableItem "{1C675205D498002A}" {
     Id "LF-RaceBoard_Title"
     Target_en_us "Track records"
     Comment "Title of the race record board card (#1925)."
    }
    CustomStringTableItem "{9212F9758808535C}" {
     Id "LF-RaceBoard_Unknown"
     Target_en_us "Unknown racer"
     Comment "Race record board: a record whose racer has no name (#1925)."
    }
    CustomStringTableItem "{2AF33ED38BCE9973}" {
     Id "LF-RaceHud_Finished"
     Target_en_us "Finished"
     Comment "Race panel next-checkpoint value once the racer crossed the finish (#1924)."
    }
    CustomStringTableItem "{059DB4A9DCC74AC6}" {
     Id "LF-RaceHud_Lap"
     Target_en_us "Lap"
     Comment "Race panel row label: the lap the racer is on, shown as 2 / 3 (#1924)."
    }
    CustomStringTableItem "{527550AA7B1AF1F6}" {
     Id "LF-RaceHud_Next"
     Target_en_us "Next checkpoint"
     Comment "Race panel row label: the next checkpoint in the lap, shown as 3 / 5 (#1924)."
    }
    CustomStringTableItem "{C140B122F6F49FB0}" {
     Id "LF-RaceHud_Over"
     Target_en_us "Race over"
     Comment "Race panel next-checkpoint value once the race has ended (#1924)."
    }
    CustomStringTableItem "{141401C4193B4C2C}" {
     Id "LF-RaceHud_Place"
     Target_en_us "Place"
     Comment "Race panel row label: the racer's position, shown as 1 / 4 (#1924)."
    }
    CustomStringTableItem "{AAC1CF2075F935EF}" {
     Id "LF-RaceHud_Time"
     Target_en_us "Time"
     Comment "Race panel row label: the race clock (#1924)."
    }
    CustomStringTableItem "{53EFFC318C712E0F}" {
     Id "LF-RaceHud_Title"
     Target_en_us "RACE"
     Comment "Heading of the racer's panel during a race (#1924)."
    }
    CustomStringTableItem "{B995FFFBC0F45EF0}" {
     Id "LF-Race_Action"
     Target_en_us "Enter the race"
     Comment "Start sign action: pay the entry fee into the pot (#1923)."
    }
    CustomStringTableItem "{848969C5F29E5FB3}" {
     Id "LF-Race_AlreadyIn"
     Target_en_us "You are already entered in a race."
     Comment "Toast: second entry refused (#1923)."
    }
    CustomStringTableItem "{B86BE62EAB387D4D}" {
     Id "LF-Race_Checkpoint"
     Target_en_us "Checkpoint. Next: %1"
     Comment "Toast: a checkpoint counted; %1 is the next checkpoint id (#1923)."
    }
    CustomStringTableItem "{CAF45E694BF8CC75}" {
     Id "LF-Race_Closing"
     Target_en_us "A racer has finished. The race ends in %1 s: get to the line."
     Comment "Toast to each racer still out when the first racer finishes. %1 = seconds until the race pays out (#2406)."
    }
    CustomStringTableItem "{6EB30AAE936633A0}" {
     Id "LF-Race_Dropped"
     Target_en_us "You were out of the race too long. You are dropped and your fee stays in the pot."
     Comment "Toast: a racer with no living body past the AFK grace is dropped from a running race (#2406)."
    }
    CustomStringTableItem "{A52B7FA55F67B76F}" {
     Id "LF-Race_Full"
     Target_en_us "This race is full."
     Comment "Toast: lobby full (#1923)."
    }
    CustomStringTableItem "{AB74D2192D4315FA}" {
     Id "LF-Race_Joined"
     Target_en_us "You paid %1 into the pot. The race starts when the lobby closes."
     Comment "Toast: entry fee paid (#1923)."
    }
    CustomStringTableItem "{82753B4F7ED6A371}" {
     Id "LF-Race_NotEnoughCash"
     Target_en_us "The entry fee is %1 in cash."
     Comment "Toast: not enough carried cash (#1923)."
    }
    CustomStringTableItem "{5F8D6B98D3F38FB3}" {
     Id "LF-Race_Prize"
     Target_en_us "You won %1 from the pot."
     Comment "Toast: race prize paid (#1923)."
    }
    CustomStringTableItem "{2000A8D4B046F01B}" {
     Id "LF-Race_Refunded"
     Target_en_us "The race is off. Your %1 fee is back in your pocket."
     Comment "Toast: race cancelled or session ending, fee refunded (#1923)."
    }
    CustomStringTableItem "{8317507B576FC3F7}" {
     Id "LF-Race_Running"
     Target_en_us "This race is already running. Wait for the next one."
     Comment "Toast: lobby closed (#1923)."
    }
    CustomStringTableItem "{30E47B1E13893274}" {
     Id "LF-Race_Started"
     Target_en_us "The race has started. Go!"
     Comment "Toast to each racer when the lobby closes (#1923)."
    }
    CustomStringTableItem "{4F32DA25EF3F27B5}" {
     Id "LF-Race_TimeUp"
     Target_en_us "Time is up. The race paid those who finished; your fee stays in the pot."
     Comment "Toast: the settle delay after the first finisher ran out while this racer was still out (#2406)."
    }
    CustomStringTableItem "{26AFE384C6B0253E}" {
     Id "LF-Race_Title"
     Target_en_us "Race"
     Comment "Notification title for every race message (#1923)."
    }
    CustomStringTableItem "{AD4539F148C6F75D}" {
     Id "LF-Race_Unavailable"
     Target_en_us "This race is not available."
     Comment "Toast: the sign's course is missing or broken (#1923)."
    }
    CustomStringTableItem "{48CF0958A82A784B}" {
     Id "LF-Radar_Clear"
     Target_en_us "Radar: %1 km/h (reading/limit). Within the limit."
     Comment "Toast: radar reading at or under the limit. %1 = speed/limit."
    }
    CustomStringTableItem "{BE2708A0DE110C62}" {
     Id "LF-Radar_Measure"
     Target_en_us "Measure speed"
     Comment "Action on the radar gun: read the speed of the car in the sights."
    }
    CustomStringTableItem "{348DA0DF7BECE3C8}" {
     Id "LF-Radar_Minor"
     Target_en_us "Radar: %1 km/h (reading/limit). Minor speeding."
     Comment "Toast: radar reading 1 to 15 over. %1 = speed/limit."
    }
    CustomStringTableItem "{C74B168D6DFC30CF}" {
     Id "LF-Radar_NoCar"
     Target_en_us "Radar: no vehicle in your sights."
     Comment "Toast: the radar gun was used with no car on the aim line."
    }
    CustomStringTableItem "{089DC48EF9D25C17}" {
     Id "LF-Radar_Road"
     Target_en_us "Radar: %1 km/h (reading/limit). Road speeding."
     Comment "Toast: radar reading 16 to 30 over. %1 = speed/limit."
    }
    CustomStringTableItem "{6BB8EC9E16D2E478}" {
     Id "LF-Radar_Serious"
     Target_en_us "Radar: %1 km/h (reading/limit). Serious speeding."
     Comment "Toast: radar reading over 30 over. %1 = speed/limit."
    }
    CustomStringTableItem "{C5ADAB68CA19613F}" {
     Id "LF-Range_Busy"
     Target_en_us "A round is already running on this lane."
     Comment "Toast refusing a second start while a round runs."
    }
    CustomStringTableItem "{2363ADEA13DC1C23}" {
     Id "LF-Range_NoShooter"
     Target_en_us "Your character is not ready for a round yet."
     Comment "Toast when the shooter has no record key yet."
    }
    CustomStringTableItem "{E473247834E47101}" {
     Id "LF-Range_NoTargets"
     Target_en_us "This lane has no targets."
     Comment "Toast when the lane has no live target."
    }
    CustomStringTableItem "{2B4E8D9FF1DEF7AD}" {
     Id "LF-Range_Result"
     Target_en_us "%1/%2 in %3 s, your best %4 s"
     Comment "Toast when a round clears the lane. %1 hits, %2 targets, %3 seconds, %4 best seconds."
    }
    CustomStringTableItem "{1A71F0442F4EF714}" {
     Id "LF-Range_StartRound"
     Target_en_us "Start a round"
     Comment "Action on a range target: starts the timed round on its lane."
    }
    CustomStringTableItem "{121111F8BA6810D0}" {
     Id "LF-Range_Started"
     Target_en_us "Round on: %1 targets, %2 s"
     Comment "Toast when a round starts. %1 targets in the lane, %2 seconds."
    }
    CustomStringTableItem "{BB662DD733C9F321}" {
     Id "LF-Range_TimeUp"
     Target_en_us "Time up: %1/%2 in %3 s"
     Comment "Toast when the 60 s run out. %1 hits, %2 targets, %3 seconds."
    }
    CustomStringTableItem "{9B59E79F970EA709}" {
     Id "LF-Range_Title"
     Target_en_us "Range"
     Comment "Title of the range toasts (Feature/Range)."
    }
    CustomStringTableItem "{D8D23856A51B44A4}" {
     Id "LF-RankBoard_None"
     Target_en_us "Nobody is on the roster yet."
     Comment "Rank board line when no officer is on the police roster (#4206)."
    }
    CustomStringTableItem "{1789A3ABA9FAD36B}" {
     Id "LF-RankBoard_Read"
     Target_en_us "Read the rank board"
     Comment "Action on the printed rank board by the duty desk (#4206)."
    }
    CustomStringTableItem "{45FB47B34F55289D}" {
     Id "LF-RankBoard_Title"
     Target_en_us "Rank board"
     Comment "Title of the printed rank board at the police station (#4206)."
    }
    CustomStringTableItem "{8B1F4E70C2A9D356}" {
     Id "LF-RankFleet_NeedsRank"
     Target_en_us "That car is for the rank of %1 and up."
     Comment "Toast (PO-R1, #4205): the police car pool refused a car above the officer's rank. %1 = the title of the lowest rank that draws it."
    }
    CustomStringTableItem "{D3A6075E19F482CB}" {
     Id "LF-RankFleet_NotIssued"
     Target_en_us "The car pool has no car for you."
     Comment "Toast (PO-R1, #4205): the police car pool has no car on its table for this rank, or the car is not one it issues."
    }
    CustomStringTableItem "{5985DB962913E1DE}" {
     Id "LF-Rank_EscortDenied"
     Target_en_us "Your rank cannot escort"
     Comment "Escort refused because the officer's rank is below the configured escort rank"
    }
    CustomStringTableItem "{37D9CCF94EDAF2EB}" {
     Id "LF-Rank_KitAboveRankTaken"
     Target_en_us "Kit above your new rank was taken back"
     Comment "Toast: after a demotion the armory took back the issued kit the officer's new rank may not hold"
    }
    CustomStringTableItem "{A2D4831F96B7DC32}" {
     Id "LF-RateLimit_Slow"
     Target_en_us "Too many requests at once. Wait a moment and try again."
     Comment "Toast body when the server refuses a request sent too fast (#693b)."
    }
    CustomStringTableItem "{AB87B08750BA98B5}" {
     Id "LF-RateLimit_Title"
     Target_en_us "Slow down"
     Comment "Toast title when the server refuses a request sent too fast (#693b)."
    }
    CustomStringTableItem "{A0F831CF41BB55DC}" {
     Id "LF-Recycler_Busy"
     Target_en_us "The bench is busy."
     Comment "Toast: an item is already on the recycler bench (#3349)."
    }
    CustomStringTableItem "{55C12BD2712F8C61}" {
     Id "LF-Recycler_Collect"
     Target_en_us "Collect the parts"
     Comment "Action: take the parts off the recycler bench (#3349)."
    }
    CustomStringTableItem "{B238E182E0A35393}" {
     Id "LF-Recycler_Collected"
     Target_en_us "You take the parts."
     Comment "Toast: the recycler parts were handed over (#3349)."
    }
    CustomStringTableItem "{C2F19F3AFF4BDE7A}" {
     Id "LF-Recycler_Load"
     Target_en_us "Load the recycler"
     Comment "Action: put an item on the recycler bench (#3349)."
    }
    CustomStringTableItem "{1EA41B7B70469311}" {
     Id "LF-Recycler_NoItem"
     Target_en_us "You do not have that item."
     Comment "Toast: the item to recycle is not in the inventory (#3349)."
    }
    CustomStringTableItem "{ED78E758588DF841}" {
     Id "LF-Recycler_NoRecipe"
     Target_en_us "Nothing you carry can be broken down here."
     Comment "Toast: no carried item has a recycle recipe (#3349)."
    }
    CustomStringTableItem "{E4540181784B2876}" {
     Id "LF-Recycler_Title"
     Target_en_us "Recycler"
     Comment "Toast title for the recycler bench (#3349)."
    }
    CustomStringTableItem "{55F29754BA4C5F7D}" {
     Id "LF-Recycler_TooFar"
     Target_en_us "Stand at the bench first."
     Comment "Toast: the worker is out of reach of the recycler (#3349)."
    }
    CustomStringTableItem "{CAFDBC0B153EF566}" {
     Id "LF-Recycler_Work"
     Target_en_us "Break it down"
     Comment "Action, held: work the recycler bench (#3349)."
    }
    CustomStringTableItem "{8810DCBD5B449AC8}" {
     Id "LF-Recycler_WrongState"
     Target_en_us "The bench has nothing to do right now."
     Comment "Toast: the recycler bench has no job to work or collect (#3349)."
    }
    CustomStringTableItem "{A45A061D749A854A}" {
     Id "LF-Reg_Age"
     Target_en_us "Age in years (18 to 80)"
     Comment "Registration form blank: age."
    }
    CustomStringTableItem "{FE48833B6120DE32}" {
     Id "LF-Reg_Error_NameChars"
     Target_en_us "Names use letters, with single spaces, hyphens, or apostrophes between them."
     Comment "Registration refused: a name has a character outside A-Z, a-z, inner space, hyphen, apostrophe."
    }
    CustomStringTableItem "{350A552D4FDC07CE}" {
     Id "LF-Reg_FirstName"
     Target_en_us "Given name"
     Comment "Registration form blank: first name."
    }
    CustomStringTableItem "{30D2235B40838FDE}" {
     Id "LF-Reg_Intro"
     Target_en_us "Print your name and age as they should appear on your identity card, then sign."
     Comment "Registration form instruction line."
    }
    CustomStringTableItem "{62351BCDA9111771}" {
     Id "LF-Reg_LastName"
     Target_en_us "Surname"
     Comment "Registration form blank: last name."
    }
    CustomStringTableItem "{3B87F299D63D5A69}" {
     Id "LF-Reg_MustSign"
     Target_en_us "Sign the form before you go into town."
     Comment "Written on the registration form when the player put it away unsigned and it came back."
    }
    CustomStringTableItem "{DDF698E219828CF0}" {
     Id "LF-Reg_Outfit"
     Target_en_us "Your clothes are your own to change: visit a clothing store once you have the money."
     Comment "Registration form hint: the look is chosen at a clothing store, not on this form."
    }
    CustomStringTableItem "{DE4D23E044F55A7E}" {
     Id "LF-Reg_Sign"
     Target_en_us "Sign the form"
     Comment "Registration form submit button."
    }
    CustomStringTableItem "{9A15FD5CEB932A07}" {
     Id "LF-Reg_Title"
     Target_en_us "Civil Registry, Form R-89"
     Comment "Registration form title on first arrival. A 1989 paper form."
    }
    CustomStringTableItem "{A422076F6E2911FE}" {
     Id "LF-Reg_Welcome"
     Target_en_us "Welcome. You are about to start a new life here: get a job, earn a living and keep to the rules you just read."
     Comment "Registration form welcome line, above the instruction. Core text is neutral; the pack overrides it."
    }
    CustomStringTableItem "{FC5B69E6943AB0F2}" {
     Id "LF-RepairKit_Action"
     Target_en_us "Repair with kit"
     Comment "Action name on a damaged vehicle (#3332)."
    }
    CustomStringTableItem "{D8DBAE73FEF4E915}" {
     Id "LF-RepairKit_Busy"
     Target_en_us "Someone is already working on this vehicle."
     Comment "Refusal (#3332)."
    }
    CustomStringTableItem "{A5624E1FC287F5D3}" {
     Id "LF-RepairKit_Cooldown"
     Target_en_us "This vehicle was only just repaired. Give it a moment."
     Comment "Refusal while the car cools down (#3332)."
    }
    CustomStringTableItem "{BA771C4080B358CC}" {
     Id "LF-RepairKit_Done"
     Target_en_us "Repaired. The kit is used up."
     Comment "Toast when the repair applies (#3332)."
    }
    CustomStringTableItem "{E2EBFA0CCD41EE95}" {
     Id "LF-RepairKit_Interrupted"
     Target_en_us "You stopped before the repair was done. The kit is kept."
     Comment "Toast when the job is dropped (#3332)."
    }
    CustomStringTableItem "{12A97ADFF80A9AB4}" {
     Id "LF-RepairKit_NeedKit"
     Target_en_us "You need a repair kit."
     Comment "Refusal (#3332)."
    }
    CustomStringTableItem "{B6A1FD9D408C30D7}" {
     Id "LF-RepairKit_NotDamaged"
     Target_en_us "This vehicle needs no repair."
     Comment "Refusal (#3332)."
    }
    CustomStringTableItem "{FBEC31E712CFA92E}" {
     Id "LF-RepairKit_Started"
     Target_en_us "You start working on the vehicle."
     Comment "Toast when the repair starts (#3332)."
    }
    CustomStringTableItem "{45F52AF20227E07B}" {
     Id "LF-RepairKit_Title"
     Target_en_us "Repair kit"
     Comment "Toast title (#3332)."
    }
    CustomStringTableItem "{87D8723475C6D323}" {
     Id "LF-RepairKit_TooFar"
     Target_en_us "Stand closer to the vehicle."
     Comment "Refusal (#3332)."
    }
    CustomStringTableItem "{5CAA2E7A7672C8C9}" {
     Id "LF-RepairKit_Wrecked"
     Target_en_us "This vehicle is wrecked beyond a kit."
     Comment "Refusal (#3332)."
    }
    CustomStringTableItem "{C80275F691522CBC}" {
     Id "LF-Reputation_Faction_Police"
     Target_en_us "The law"
     Comment "Standing: neutral core label of the police faction. The pack overlays the words."
    }
    CustomStringTableItem "{AE91EA1CF28E3574}" {
     Id "LF-Reputation_Faction_Town"
     Target_en_us "Locals"
     Comment "Standing: neutral core label of the town faction. The pack overlays the words."
    }
    CustomStringTableItem "{B97574F5460AB27E}" {
     Id "LF-Reputation_Faction_Underworld"
     Target_en_us "Underworld"
     Comment "Standing: neutral core label of the underworld faction. Never shown to the player."
    }
    CustomStringTableItem "{5D82F0B6C3A19E47}" {
     Id "LF-Reputation_File_Line"
     Target_en_us "%1: %2."
     Comment "Standing in words (#2705), on the ID card and from the clerk. %1 = the faction label, %2 = the tier word. Never a number. The pack overlays the words."
    }
    CustomStringTableItem "{F38A5D16B04E927C}" {
     Id "LF-Reputation_Refuse_Any"
     Target_en_us "You are not well enough known for this yet."
     Comment "Standing gate refusal (#2701) for a faction with no words of its own. Never a number."
    }
    CustomStringTableItem "{91E4B7035CA2D68F}" {
     Id "LF-Reputation_Refuse_Police"
     Target_en_us "The law doesn't trust you enough for this yet. You would need to be at least: %1."
     Comment "Standing gate refusal (#2701). %1 = the minimum tier word. Never a number. The pack overlays the words."
    }
    CustomStringTableItem "{2C7A91E45B03D8F6}" {
     Id "LF-Reputation_Refuse_Town"
     Target_en_us "The locals don't know you well enough for this work yet. You would need to be at least: %1."
     Comment "Standing gate refusal (#2701): job sign-up or run start. %1 = the minimum tier word. Never a number. The pack overlays the words."
    }
    CustomStringTableItem "{6D0F3A82E715C4B9}" {
     Id "LF-Reputation_Refuse_Underworld"
     Target_en_us "Nobody in those circles knows you well enough."
     Comment "Standing gate refusal (#2701). The underworld's tier is never shown, so no tier word. The pack overlays the words."
    }
    CustomStringTableItem "{067374E8D35098CE}" {
     Id "LF-Reputation_Tier_0"
     Target_en_us "Unknown"
     Comment "Standing: neutral core label of tier 0. The pack overlays the words."
    }
    CustomStringTableItem "{EB0E8494CA8497EB}" {
     Id "LF-Reputation_Tier_1"
     Target_en_us "Known"
     Comment "Standing: neutral core label of tier 1. The pack overlays the words."
    }
    CustomStringTableItem "{F014E5DE1DBB6EB7}" {
     Id "LF-Reputation_Tier_2"
     Target_en_us "Trusted"
     Comment "Standing: neutral core label of tier 2. The pack overlays the words."
    }
    CustomStringTableItem "{37CF03FD90EA2650}" {
     Id "LF-Reputation_Tier_3"
     Target_en_us "Respected"
     Comment "Standing: neutral core label of tier 3. The pack overlays the words."
    }
    CustomStringTableItem "{33DDEE3DC227F790}" {
     Id "LF-Reputation_Tier_Neg1"
     Target_en_us "Doubted"
     Comment "Standing: neutral core label of tier -1. The pack overlays the words."
    }
    CustomStringTableItem "{00CAD4E0C49A2EC2}" {
     Id "LF-Reputation_Tier_Neg2"
     Target_en_us "Distrusted"
     Comment "Standing: neutral core label of tier -2. The pack overlays the words."
    }
    CustomStringTableItem "{3783CFF6EFED5F5A}" {
     Id "LF-Reputation_Toast_Police_Down"
     Target_en_us "The {police} have started to keep an eye on you"
     Comment "Standing toast: the police tier went down. Words only, never a number."
    }
    CustomStringTableItem "{9CA1505D71AF22D9}" {
     Id "LF-Reputation_Toast_Police_Up"
     Target_en_us "The {police} have started to see you as one of the good ones"
     Comment "Standing toast: the police tier went up. Words only, never a number."
    }
    CustomStringTableItem "{B879A4BDE2FB466F}" {
     Id "LF-Reputation_Toast_Title"
     Target_en_us "Standing"
     Comment "Standing: title of the toast when a standing tier changes."
    }
    CustomStringTableItem "{88053A4E9492D71B}" {
     Id "LF-Reputation_Toast_Town_Down"
     Target_en_us "People in {town} have started to keep their distance"
     Comment "Standing toast: the town tier went down. Words only, never a number."
    }
    CustomStringTableItem "{A0995D5E8C36B097}" {
     Id "LF-Reputation_Toast_Town_Up"
     Target_en_us "People in {town} have started to trust you"
     Comment "Standing toast: the town tier went up. Words only, never a number."
    }
    CustomStringTableItem "{51C3BFFF112B62A7}" {
     Id "LF-ResidentNeed_Chain_Desc"
     Target_en_us "Stock is low. Bring me three apples from the orchards."
     Comment "Quest description: resident need, a resource-chain good"
    }
    CustomStringTableItem "{60C16CF46A591B02}" {
     Id "LF-ResidentNeed_Chain_Title"
     Target_en_us "Fruit for the counter"
     Comment "Quest title: a resident asks for apples"
    }
    CustomStringTableItem "{0071867DE38C89B3}" {
     Id "LF-ResidentNeed_Chain_TurnIn"
     Target_en_us "Fresh ones, thank you. I will not forget it."
     Comment "Resident line on turn-in of the apples need"
    }
    CustomStringTableItem "{5FC167C3738D168E}" {
     Id "LF-ResidentNeed_Deliver_Desc"
     Target_en_us "I have been on my feet all morning. Could you bring me a bottle of water?"
     Comment "Quest description: resident need, deliver an item to the counter"
    }
    CustomStringTableItem "{08028F6352412F75}" {
     Id "LF-ResidentNeed_Deliver_Title"
     Target_en_us "Bring me some water"
     Comment "Quest title: a resident asks for a bottle of water"
    }
    CustomStringTableItem "{3511F3475812F95D}" {
     Id "LF-ResidentNeed_Deliver_TurnIn"
     Target_en_us "That hits the spot. I will remember this."
     Comment "Resident line on turn-in of the water need"
    }
    CustomStringTableItem "{028A523E8AB37314}" {
     Id "LF-ResidentNeed_Refused"
     Target_en_us "I have nothing for you. Not after what I have heard."
     Comment "Toast: a resident who refuses the player offers no errand"
    }
    CustomStringTableItem "{C96808FBA0407AFE}" {
     Id "LF-Restrain_Action"
     Target_en_us "Cuff"
     Comment "Interaction menu entry for an on-duty officer holding handcuffs: cuff the person in front of you."
    }
    CustomStringTableItem "{8865F05A2F144187}" {
     Id "LF-Restrain_Already"
     Target_en_us "Already in handcuffs."
     Comment "Cuff refusal: the target is cuffed already."
    }
    CustomStringTableItem "{A39AC47B9AF1F8B0}" {
     Id "LF-Restrain_Cuffed"
     Target_en_us "Suspect cuffed."
     Comment "Toast to the officer after cuffing someone."
    }
    CustomStringTableItem "{DF8BF4F56126B7CA}" {
     Id "LF-Restrain_Interrupted"
     Target_en_us "The cuffing was interrupted."
     Comment "Toast to the officer when the cuff or uncuff hold stops by itself: someone moved out of reach, the cuffs left the hand, or a rule no longer holds."
    }
    CustomStringTableItem "{677F023211DC8C42}" {
     Id "LF-Restrain_NoCuffs"
     Target_en_us "Hold handcuffs in your hand first."
     Comment "Cuff refusal: the officer holds no handcuffs."
    }
    CustomStringTableItem "{B261D371541EF2AF}" {
     Id "LF-Restrain_NotCompliant"
     Target_en_us "Only a downed suspect, or one held up who holds no weapon, can be cuffed."
     Comment "Cuff refusal: the target is standing, not held up, or armed."
    }
    CustomStringTableItem "{4F1BD3FDEFA49623}" {
     Id "LF-Restrain_NotCuffed"
     Target_en_us "Not in handcuffs."
     Comment "Uncuff refusal: the target is not cuffed."
    }
    CustomStringTableItem "{43F938FE3194371E}" {
     Id "LF-Restrain_NotOfficer"
     Target_en_us "Only an on-duty officer can do that."
     Comment "Cuff refusal: the player is not clocked in as police."
    }
    CustomStringTableItem "{7C2E91A5D3B04F68}" {
     Id "LF-Restrain_NotWhileCuffed"
     Target_en_us "Not while in handcuffs."
     Comment "Toast to a cuffed player whose request the server refused: a robbery, a lockpick, a driver's seat, giving cash or an item."
    }
    CustomStringTableItem "{1069F4CF50C7B5A1}" {
     Id "LF-Restrain_Release"
     Target_en_us "Uncuff"
     Comment "Interaction menu entry for an on-duty officer facing a cuffed player: take the handcuffs off."
    }
    CustomStringTableItem "{5E8975FFDDC6A07C}" {
     Id "LF-Restrain_Released"
     Target_en_us "The handcuffs are off."
     Comment "Toast to both the officer and the player when the handcuffs come off."
    }
    CustomStringTableItem "{421ECB9A8B26A772}" {
     Id "LF-Restrain_Title"
     Target_en_us "Handcuffs"
     Comment "Toast title for cuffing and uncuffing."
    }
    CustomStringTableItem "{4861834BE14FFC3F}" {
     Id "LF-Restrain_YouAreCuffed"
     Target_en_us "You are in handcuffs."
     Comment "Toast to the player who was just cuffed."
    }
    CustomStringTableItem "{B18A9253EC649E50}" {
     Id "LF-Revive_Action"
     Target_en_us "Revive"
     Comment "Player action on a downed player, offered to an on-duty medic."
    }
    CustomStringTableItem "{D4A0B004F3AE2A4B}" {
     Id "LF-Revive_Done"
     Target_en_us "You brought them back."
     Comment "Toast to the medic after a revive."
    }
    CustomStringTableItem "{F0336D48D1263225}" {
     Id "LF-Revive_MedicDown"
     Target_en_us "You cannot help anyone while you are down."
     Comment "Revive refusal: the medic is downed or dead."
    }
    CustomStringTableItem "{7A30C5E1D49B6B01}" {
     Id "LF-Revive_NoItem"
     Target_en_us "You need a field dressing to revive someone."
     Comment "Revive refusal: the medic has none of the configured revive item."
    }
    CustomStringTableItem "{943F121585F92A15}" {
     Id "LF-Revive_NotDowned"
     Target_en_us "They are not down."
     Comment "Revive refusal: the target is not downed."
    }
    CustomStringTableItem "{4AC63DA91F04B8C1}" {
     Id "LF-Revive_NotMedic"
     Target_en_us "Only a {medic} on duty can revive."
     Comment "Revive refusal: the user is not clocked in as a medic."
    }
    CustomStringTableItem "{53CD088BE893DA58}" {
     Id "LF-Revive_Revived"
     Target_en_us "A {medic} brought you back. You are weak: take it easy."
     Comment "Toast to the revived player."
    }
    CustomStringTableItem "{9DE20E9DD947A51A}" {
     Id "LF-Revive_TooFar"
     Target_en_us "Too far away to revive them."
     Comment "Revive refusal: out of range."
    }
    CustomStringTableItem "{4E1B7A92C60D3F58}" {
     Id "LF-RoadMap_ItemDesc"
     Target_en_us "A folded road map of the island. Study it to pencil every settlement onto your own map."
     Comment "Road map item (#4474): the item description."
    }
    CustomStringTableItem "{9D3C05E2B8A41F76}" {
     Id "LF-RoadMap_ItemName"
     Target_en_us "Road map"
     Comment "Road map item (#4474): the item name."
    }
    CustomStringTableItem "{2F86B14D7C0A59E3}" {
     Id "LF-RoadMap_Marked"
     Target_en_us "You pencil %1 new settlements onto your map."
     Comment "Road map item (#4474): toast after studying it. %1 = towns new to the player's map."
    }
    CustomStringTableItem "{B7054E3A91D2C86F}" {
     Id "LF-RoadMap_Nothing"
     Target_en_us "Your map already shows every settlement this one lists."
     Comment "Road map item (#4474): toast when it adds nothing."
    }
    CustomStringTableItem "{51C9A07E3B6D2F84}" {
     Id "LF-RoadMap_Title"
     Target_en_us "Road map"
     Comment "Road map item (#4474): the toast title."
    }
    CustomStringTableItem "{C60E83B52A7D194F}" {
     Id "LF-RoadMap_Use"
     Target_en_us "Study the road map"
     Comment "Road map item (#4474): the action that pencils every live town onto the player's map."
    }
  CustomStringTableItem "{BABA49E2440A6362}" {
   Id "LF-Roadblock_Deploy"
   Target_en_us "Deploy roadblock"
   Comment "Action on the folded roadblock kit: lays the barrier and cones across the road in front of the officer (#1941)."
  }
  CustomStringTableItem "{3655F9540576258C}" {
   Id "LF-Roadblock_Description"
   Target_en_us "Folding roadblock kit: a barrier and four cones. Drop it at the roadside and deploy it across the road; pick any piece up to fold the set away. On-duty officers only."
   Comment "Inventory description of the police roadblock kit. The engine renders a prefab description itself, so the text holds no term placeholder (validate-term-sinks)."
  }
  CustomStringTableItem "{986CA09EB5046245}" {
   Id "LF-Roadblock_Name"
   Target_en_us "Roadblock kit"
   Comment "Inventory name of the police roadblock kit (#1941)."
  }
  CustomStringTableItem "{333125FDBDB896DE}" {
   Id "LF-Roadblock_PickUp"
   Target_en_us "Pick up roadblock"
   Comment "Action on a piece of a deployed roadblock: folds the whole set back into the officer inventory."
  }
  CustomStringTableItem "{440F14D84078D338}" {
   Id "LF-Roadblock_PoliceOnly"
   Target_en_us "Only an on-duty officer can handle the roadblock kit."
   Comment "Toast and greyed-out reason when someone who is not on-duty police tries the roadblock kit."
  }
    CustomStringTableItem "{94A1952EED77A6E8}" {
     Id "LF-RobHold_ClerkBeg"
     Target_en_us "Please, I am not a hero. I will not call anyone, I swear it. Just take it."
     Comment "Caption of a fuel attendant line spoken during a robbery (voiced: tools/assets/dialogue/fuel_clerk)."
    }
    CustomStringTableItem "{EFA416C7403C71CF}" {
     Id "LF-RobHold_ClerkGone"
     Target_en_us "He is gone? Oh God. Somebody call the police!"
     Comment "Caption of a fuel attendant line spoken during a robbery (voiced: tools/assets/dialogue/fuel_clerk)."
    }
    CustomStringTableItem "{AB28C43BB756D769}" {
     Id "LF-RobHold_ClerkPaid"
     Target_en_us "There! That is all of it, all of it! Take it and go, please, just go!"
     Comment "Caption of a fuel attendant line spoken during a robbery (voiced: tools/assets/dialogue/fuel_clerk)."
    }
    CustomStringTableItem "{2040F7D3825AA900}" {
     Id "LF-RobHold_ClerkPolice"
     Target_en_us "Police! In here! He has a gun! Help me!"
     Comment "Caption of a fuel attendant line spoken during a robbery (voiced: tools/assets/dialogue/fuel_clerk)."
    }
    CustomStringTableItem "{0F045151110132ED}" {
     Id "LF-RobHold_ClerkStammer"
     Target_en_us "My hands, I cannot stop my hands. It is coming, it is almost open, please..."
     Comment "Caption of a fuel attendant line spoken during a robbery (voiced: tools/assets/dialogue/fuel_clerk)."
    }
    CustomStringTableItem "{0846C4E67EC47B16}" {
     Id "LF-RobHold_Edge"
     Target_en_us "Too far! Back to the counter: %2 m of %1 m"
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c). Shown near the edge of the radius. %1 is the radius, %2 the distance."
    }
    CustomStringTableItem "{1383AADB0F8627B0}" {
     Id "LF-RobHold_Fail_Clerk"
     Target_en_us "Robbery failed: the attendant went down."
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c)."
    }
    CustomStringTableItem "{C823A75E1A8C79F8}" {
     Id "LF-RobHold_Fail_Downed"
     Target_en_us "Robbery failed: you went down."
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c)."
    }
    CustomStringTableItem "{8851882B617E8962}" {
     Id "LF-RobHold_Fail_Left"
     Target_en_us "Robbery failed: you left the counter."
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c)."
    }
    CustomStringTableItem "{D6A9DD47C75E011F}" {
     Id "LF-RobHold_Fail_Police"
     Target_en_us "Robbery failed: the {police} arrived."
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c)."
    }
    CustomStringTableItem "{90B712FFFF5477C3}" {
     Id "LF-RobHold_Fail_Till"
     Target_en_us "Robbery failed: the till could not pay out."
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c)."
    }
    CustomStringTableItem "{985FFEE6750F2BA2}" {
     Id "LF-RobHold_Fail_Weapon"
     Target_en_us "Robbery failed: you put your weapon away."
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c)."
    }
    CustomStringTableItem "{653858F0C99AEAC1}" {
     Id "LF-RobHold_Range"
     Target_en_us "Stay within %1 m of the till. You are %2 m away."
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c). %1 is the radius, %2 the distance."
    }
    CustomStringTableItem "{7A61B157428F9D30}" {
     Id "LF-RobHold_Refused_Busy"
     Target_en_us "(This till is already being robbed.)"
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c). Caption when the robbery cannot start."
    }
    CustomStringTableItem "{2014B40942A90812}" {
     Id "LF-RobHold_Refused_Civilian"
     Target_en_us "(Only civilians can rob.)"
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c). Caption when the robbery cannot start."
    }
    CustomStringTableItem "{008CB39C11D31B0D}" {
     Id "LF-RobHold_Refused_Cooldown"
     Target_en_us "(The till was emptied a short while ago. Come back later.)"
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c). Caption when the robbery cannot start."
    }
    CustomStringTableItem "{D455F915F3D9F8DF}" {
     Id "LF-RobHold_Refused_Holding"
     Target_en_us "(You are already holding up another till.)"
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c). Caption when the robbery cannot start."
    }
    CustomStringTableItem "{66AD4C2125D11C00}" {
     Id "LF-RobHold_Refused_NoTill"
     Target_en_us "(There is no till here to rob.)"
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c). Caption when the robbery cannot start."
    }
    CustomStringTableItem "{0078A75E091ED0E8}" {
     Id "LF-RobHold_Refused_Police"
     Target_en_us "(Not enough {police} on duty for a robbery.)"
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c). Caption when the robbery cannot start."
    }
    CustomStringTableItem "{670B9380BE43A60E}" {
     Id "LF-RobHold_Refused_Rate"
     Target_en_us "(Too soon. Wait a moment.)"
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c). Caption when the robbery cannot start."
    }
    CustomStringTableItem "{34CD07CABFFB2080}" {
     Id "LF-RobHold_Refused_Weapon"
     Target_en_us "(You need a weapon drawn.)"
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c). Caption when the robbery cannot start."
    }
    CustomStringTableItem "{09906F4013CF0802}" {
     Id "LF-RobHold_Rules"
     Target_en_us "It fails if you leave, put your weapon away, go down, or the police arrive."
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c)."
    }
    CustomStringTableItem "{917C7F209A2E5AE2}" {
     Id "LF-RobHold_Success"
     Target_en_us "The till is open. You took the cash."
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c)."
    }
    CustomStringTableItem "{A117438990F577D9}" {
     Id "LF-RobHold_Timer"
     Target_en_us "Keep the clerk at gunpoint: %1"
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c). %1 is the time left, as 0:20."
    }
    CustomStringTableItem "{FE6717CB3E1F9AF6}" {
     Id "LF-RobHold_Title"
     Target_en_us "ROBBERY"
     Comment "Fuel station robbery through the attendant's dialogue (Feature/Crime/LF_RobHold.c). Header of the hold panel."
    }
    CustomStringTableItem "{CAE4963AFB138F1B}" {
     Id "LF-Rob_NeedPoliceCount"
     Target_en_us "At least %1 {police} officers must be on duty"
     Comment "Vehicle/weapon robbery refused because too few police are on duty. %1 = required count"
    }
    CustomStringTableItem "{F72676CB2A9E8ED9}" {
     Id "LF-Rob_RecentlyRobbed"
     Target_en_us "Robbed recently. Lie low for now."
     Comment "Vehicle/weapon robbery refused because the cooldown is still running"
    }
    CustomStringTableItem "{3B9E05C7A1D48F62}" {
     Id "LF-Robbery_NoRoom"
     Target_en_us "No room for the cash. The till stays shut."
     Comment "Toast: the robber's inventory cannot take the till cash, so nothing is taken and nobody is alerted (crime review 2026-11-17)."
    }
    CustomStringTableItem "{4461B0C3D5E70005}" {
     Id "LF-RumorTalk_AskedToday"
     Target_en_us "I have told you what I know today. Ask again tomorrow."
     Comment "Spoken caption when you ask the same giver twice in one game day (#4461)."
    }
    CustomStringTableItem "{4461B0C3D5E70004}" {
     Id "LF-RumorTalk_NothingNew"
     Target_en_us "Nothing new. Ask me tomorrow."
     Comment "Spoken caption when the giver has no rumor you do not already know (#4461)."
    }
    CustomStringTableItem "{4461B0C3D5E70002}" {
     Id "LF-RumorTalk_Title"
     Target_en_us "Word on the street"
     Comment "Toast title when a resident tells a rumor (#4461)."
    }
    CustomStringTableItem "{4461B0C3D5E70003}" {
     Id "LF-RumorTalk_Told"
     Target_en_us "They told you about a place. It is on your map now."
     Comment "Toast body for a told rumor whose row has no caption of its own (#4461)."
    }
    CustomStringTableItem "{98F59847428914BF}" {
     Id "LF-RunPlate_Action"
     Target_en_us "Run plate"
     Comment "Action row on a police car (#4425): read the plate of the car ahead."
    }
    CustomStringTableItem "{C945EFD33C6582C6}" {
     Id "LF-RunPlate_Clear"
     Target_en_us "CLEAR"
     Comment "Pager word: registered, no report (#4425)."
    }
    CustomStringTableItem "{FF039827A68F2E8B}" {
     Id "LF-RunPlate_Line"
     Target_en_us "Plate %1: %2"
     Comment "Pager line after Run plate (#4425). %1 = plate, %2 = STOLEN, CLEAR or UNKNOWN."
    }
    CustomStringTableItem "{2DDB8B7DB753748B}" {
     Id "LF-RunPlate_NoCar"
     Target_en_us "No car ahead within 30 meters."
     Comment "Toast: run plate found no car in front (#4425)."
    }
    CustomStringTableItem "{07ED6567EF4AD283}" {
     Id "LF-RunPlate_NotDriving"
     Target_en_us "Run a plate from the driver seat of a patrol car."
     Comment "Toast: run plate refused, the officer is not at the wheel (#4425)."
    }
    CustomStringTableItem "{AE1F8B3239967066}" {
     Id "LF-RunPlate_OffDuty"
     Target_en_us "Only an on-duty officer can run a plate."
     Comment "Toast: run plate refused, the officer is not on duty (#4425)."
    }
    CustomStringTableItem "{2F32BB51C0269D79}" {
     Id "LF-RunPlate_Stolen"
     Target_en_us "STOLEN"
     Comment "Pager word: the plate is on the stolen-car hotlist (#4425)."
    }
    CustomStringTableItem "{F27DCCD7B370563F}" {
     Id "LF-RunPlate_Unknown"
     Target_en_us "UNKNOWN"
     Comment "Pager word: the plate is on no register (#4425)."
    }
    CustomStringTableItem "{4AA0194A741E081B}" {
     Id "LF-SafeZone_ShotRefused"
     Target_en_us "The {town} square is a safe zone. Your shot hurt nobody."
     Comment "Toast to a player whose shot was refused in or into the spawn safe zone. Rate-limited."
    }
    CustomStringTableItem "{3DCC039CAC772B0D}" {
     Id "LF-SafeZone_Title"
     Target_en_us "Safe zone"
     Comment "Toast title when a shot is refused in the spawn safe zone."
    }
    CustomStringTableItem "{4EE60B1CA1D7C2F3}" {
     Id "LF-Safe_BoxRob"
     Target_en_us "Crack the box safe"
     Comment "Action: a small box safe in a shop (safe type store_box)."
    }
    CustomStringTableItem "{E04945AFDC28CB9A}" {
     Id "LF-Safe_ReserveRob"
     Target_en_us "Break into the reserve depot"
     Comment "Action: the big standalone vault (safe type reserve_vault). Neutral name; the docs call the building the Federal Reserve."
    }
    CustomStringTableItem "{59E2E4A1484842B5}" {
     Id "LF-Scanner_Heard"
     Target_en_us "Heard on the scanner: %1"
     Comment "Toast when a scanner hears a service call; %1 is the call summary."
    }
    CustomStringTableItem "{4A5D20C00F349F20}" {
     Id "LF-Scanner_Title"
     Target_en_us "Scanner"
     Comment "Toast title for a scanner."
    }
    CustomStringTableItem "{5EA2C4A000000003}" {
     Id "LF-Search_Clean"
     Target_en_us "Search found nothing"
     Comment "Officer search found no chain goods (#419)."
    }
    CustomStringTableItem "{5EA2C4A000000002}" {
     Id "LF-Search_Found"
     Target_en_us "Contraband found and seized"
     Comment "Officer search found chain goods (#419)."
    }
    CustomStringTableItem "{3427B2E1D0F95C28}" {
     Id "LF-Search_Full_Action"
     Target_en_us "Full search"
     Comment "Vehicle action: the detective's full search, hidden compartment included (#3427)."
    }
    CustomStringTableItem "{2154A1C0B7D34E01}" {
     Id "LF-Search_SeizedCount"
     Target_en_us "%1x %2"
     Comment "A seized item kind with its count; %1 is the count, %2 the item name (#2154)."
    }
    CustomStringTableItem "{2154A1C0B7D34E02}" {
     Id "LF-Search_SeizedFrom"
     Target_en_us "Seized from %1: %2"
     Comment "Officer toast after a search; %1 is the suspect's name, %2 the seized items (#2154)."
    }
    CustomStringTableItem "{2154A1C0B7D34E03}" {
     Id "LF-Search_SeizedMore"
     Target_en_us "%1 and %2 more"
     Comment "Seized list cut to three kinds; %1 is the named kinds, %2 how many more (#2154)."
    }
    CustomStringTableItem "{FE96F6E326947DD1}" {
     Id "LF-Search_TakenFromYou"
     Target_en_us "The {police} took: %1"
     Comment "Toast to the searched player: what the police search seized. %1 = the item list, \"A, B and 2 more\" (agent playtest 2026-11-27)."
    }
    CustomStringTableItem "{31401FB7D80A1637}" {
     Id "LF-Search_TakenPlain"
     Target_en_us "The {police} took goods you may not carry."
     Comment "Toast to the searched player when the search took only chain goods, which have no item names."
    }
    CustomStringTableItem "{6B2AEFE8DB63D32F}" {
     Id "LF-Search_UnknownSuspect"
     Target_en_us "an unknown person"
     Comment "Inserted as %1 in LF-Search_SeizedFrom (Seized from %1: %2) when the suspect never introduced themselves to the officer (#5145)."
    }
    CustomStringTableItem "{5EA2C4A000000004}" {
     Id "LF-Search_Vehicle_Action"
     Target_en_us "Search vehicle"
     Comment "Vehicle action: officer searches the storage for chain goods (#419)."
    }
    CustomStringTableItem "{15EDE2A4CCF09558}" {
     Id "LF-Serial_Read"
     Target_en_us "Serial: %1"
     Comment "Toast text when a fence tool serial is read (#272)."
    }
    CustomStringTableItem "{0FE08C6138289656}" {
     Id "LF-Serial_Read_Action"
     Target_en_us "Read serial"
     Comment "Action name to read a fence tool serial (#272)."
    }
    CustomStringTableItem "{3BD89A2DB71401CE}" {
     Id "LF-Serial_Title"
     Target_en_us "Serial number"
     Comment "Toast title when a fence tool serial is read (#272)."
    }
    CustomStringTableItem "{7A099928BD809AE7}" {
     Id "LF-Service_Medic"
     Target_en_us "{Medic}"
     Comment "Service name callers see for the medic service."
    }
    CustomStringTableItem "{719C7271EF84BC5B}" {
     Id "LF-Service_NoAnswer"
     Target_en_us "No answer. Nobody is on duty for that service right now."
     Comment "Shown to a caller when nobody is on duty for the service they called (#248)."
    }
    CustomStringTableItem "{869A6F273AAC96A0}" {
     Id "LF-Service_Police"
     Target_en_us "{Police}"
     Comment "Service name callers see for the police service."
    }
    CustomStringTableItem "{46622CE72B35428C}" {
     Id "LF-Service_Taxi"
     Target_en_us "Taxi"
     Comment "Service name callers see for the taxi service."
    }
    CustomStringTableItem "{1AD33D680D4FC3B9}" {
     Id "LF-Service_UnitOnTheWay"
     Target_en_us "A unit is on the way."
     Comment "Shown to a caller when a responder takes their call (#251)."
    }
    CustomStringTableItem "{8DE2406C7DDB39E2}" {
     Id "LF-Service_UnitTitle"
     Target_en_us "Dispatch"
     Comment "Title of dispatch toasts."
    }
    CustomStringTableItem "{83D3279735869111}" {
     Id "LF-ShiftBoard_NoSite"
     Target_en_us "This board belongs to no work site."
     Comment "Shift board line when the board stands at no workplace (#4712)."
    }
    CustomStringTableItem "{36443057824AB08E}" {
     Id "LF-ShiftBoard_None"
     Target_en_us "Nobody has worked here this week yet."
     Comment "Shift board line when no XP was earned at the site this week (#4712)."
    }
    CustomStringTableItem "{6765440F09DD00A1}" {
     Id "LF-ShiftBoard_Read"
     Target_en_us "Read the shift board"
     Comment "Action on a work site's printed shift board (#4712)."
    }
    CustomStringTableItem "{89B6F4DEDEDE2542}" {
     Id "LF-ShiftBoard_Row"
     Target_en_us "%1. %2, %3 %4, %5 XP"
     Comment "One row of the shift board (#4712). %1 = place, %2 = worker name, %3 = trade, %4 = trade level, %5 = XP earned at this site this week. Example: 1. Ada Newcomer, Mining 7, 1,240 XP"
    }
    CustomStringTableItem "{F6462EE479F213FD}" {
     Id "LF-ShiftBoard_Title"
     Target_en_us "This week's shift board"
     Comment "Title of a work site's printed shift board (WK-W22, #4712)."
    }
    CustomStringTableItem "{BFADD852A420193D}" {
     Id "LF-ShiftBoard_Unnamed"
     Target_en_us "A worker"
     Comment "Shift board name when the server knows no name for a worker (#4712)."
    }
    CustomStringTableItem "{5B17BCD5B61026B6}" {
     Id "LF-Shop_Browse"
     Target_en_us "Browse goods"
     Comment "Interaction name on a shopkeeper NPC or stall: opens the shop's price list."
    }
    CustomStringTableItem "{C2D8EF8EC8E7CAC8}" {
     Id "LF-Shop_CashLine"
     Target_en_us "Cash in hand: %1"
     Comment "Shop price list line for the cash the player carries. %1 is the formatted amount."
    }
    CustomStringTableItem "{C600431286D021E1}" {
     Id "LF-Shop_Desc_BackRoom"
     Target_en_us "Special equipment"
     Target_es_es "Equipamiento especial"
     Comment "DebugWorld black-market shop description"
    }
    CustomStringTableItem "{2B108325B520AB97}" {
     Id "LF-Shop_Desc_Chandler"
     Target_en_us "Rods and tins of bait."
     Comment "Shop description of the chandler (#2995)."
    }
    CustomStringTableItem "{78CFC7A314AD1790}" {
     Id "LF-Shop_Desc_Fence"
     Target_en_us "Tools, no questions. If he knows you."
     Comment "Fence shop description (#271)."
    }
    CustomStringTableItem "{637D24F33F4A818B}" {
     Id "LF-Shop_Desc_FishCounter"
     Target_en_us "Sell your catch and buy bait."
     Comment "Shop description of the fish counter (#2995)."
    }
    CustomStringTableItem "{595445544B0E7D7B}" {
     Id "LF-Shop_Desc_General"
     Target_en_us "General supplies"
     Target_es_es "Tienda general de suministros"
     Comment "DebugWorld shop description"
    }
    CustomStringTableItem "{9FF131B1A71F8990}" {
     Id "LF-Shop_Desc_GunCounter"
     Target_en_us "Handguns for permit holders."
     Comment "Shop description of the licensed gun counter outlet (#988)."
    }
    CustomStringTableItem "{F2D0DCE7D7861628}" {
     Id "LF-Shop_Desc_Supermarket"
     Target_en_us "Groceries and drinks"
     Comment "Shop description of a village supermarket till (#3219)."
    }
    CustomStringTableItem "{B4FBE7394CA00159}" {
     Id "LF-Shop_Hint_General"
     Target_en_us "Looking for something stronger? Try the back room at the head of the cobbled alley, east of the square. Knock, and bring cash."
     Comment "The general store keeper's word on his sheet: where the back room is (#933)."
    }
    CustomStringTableItem "{1B9D952889159031}" {
     Id "LF-Shop_Item_M9"
     Target_en_us "M9 pistol"
     Comment "Shop item display name"
    }
    CustomStringTableItem "{934DA7DD0FA38AC9}" {
     Id "LF-Shop_Leave"
     Target_en_us "Leave"
     Comment "Shop price list key that closes it."
    }
    CustomStringTableItem "{6BEE9A234FAE3042}" {
     Id "LF-Shop_Less"
     Target_en_us "One less"
     Comment "Shop order slip key: lower the quantity by one."
    }
    CustomStringTableItem "{17C9F77EFE1AEE8C}" {
     Id "LF-Shop_LicenceNoSellBack"
     Target_en_us "A permit cannot be sold back."
     Comment "Shop price list: Sell pressed with a licence row on the slip."
    }
    CustomStringTableItem "{02FFEC0ECB74AFC2}" {
     Id "LF-Shop_LookFor"
     Target_en_us "Look for an item ..."
     Comment "Shop price list search field placeholder; typing narrows the list."
    }
    CustomStringTableItem "{0314921F9811C286}" {
     Id "LF-Shop_More"
     Target_en_us "One more"
     Comment "Shop order slip key: raise the quantity by one."
    }
    CustomStringTableItem "{C23FF12F6B1296E1}" {
     Id "LF-Shop_Name_BackRoom"
     Target_en_us "Back room"
     Target_es_es "Mercado negro"
     Comment "DebugWorld black-market shop name"
    }
    CustomStringTableItem "{08A9491DE03AD84D}" {
     Id "LF-Shop_Name_Chandler"
     Target_en_us "Chandler"
     Comment "Shop name of the chandler outlet that sells rods and bait (#2995)."
    }
    CustomStringTableItem "{6190A2B8AD6F7862}" {
     Id "LF-Shop_Name_Fence"
     Target_en_us "The Fence"
     Comment "Fence shop name (#271)."
    }
    CustomStringTableItem "{103833B3B4711CB4}" {
     Id "LF-Shop_Name_FenceStall"
     Target_en_us "Under the counter"
     Comment "Title of the fence stall's price list."
    }
    CustomStringTableItem "{54974BC934AC8354}" {
     Id "LF-Shop_Name_FishCounter"
     Target_en_us "Fish counter"
     Comment "Shop name of the fish counter outlet that buys the catch (#2995)."
    }
    CustomStringTableItem "{9B2FF881F1CB91D3}" {
     Id "LF-Shop_Name_General"
     Target_en_us "General store"
     Target_es_es "Tienda general"
     Comment "DebugWorld shop name"
    }
    CustomStringTableItem "{65BB7224C4B126D9}" {
     Id "LF-Shop_Name_GunCounter"
     Target_en_us "Gun counter"
     Comment "Shop name of the licensed gun counter outlet (#988)."
    }
    CustomStringTableItem "{CB9900E42434661E}" {
     Id "LF-Shop_Name_Supermarket"
     Target_en_us "Supermarket"
     Comment "Shop name of a village supermarket till (#3219)."
    }
    CustomStringTableItem "{684633A0ED601DA3}" {
     Id "LF-Shop_NoReply"
     Target_en_us "The till did not answer. Try again."
     Comment "Shop slip line when the server did not answer within 5 seconds."
    }
    CustomStringTableItem "{3F69457F6AC513EA}" {
     Id "LF-Shop_Offer"
     Target_en_us "Offer"
     Comment "Shop price list row button on a line the counter buys: puts it on the slip to sell."
    }
    CustomStringTableItem "{F1C7C0F870099A03}" {
     Id "LF-Shop_OpenDesc_BackRoom"
     Target_en_us "Back room. Cash only."
     Target_es_es "Comprar equipamiento"
     Comment "Interaction description on the black-market shop"
    }
    CustomStringTableItem "{962AD3A70D1AF2CA}" {
     Id "LF-Shop_OpenDesc_Fence"
     Target_en_us "See what he will sell you"
     Comment "Interaction description on the fence NPC (#271)."
    }
    CustomStringTableItem "{1A64603286E76D07}" {
     Id "LF-Shop_OpenDesc_General"
     Target_en_us "Buy food and supplies"
     Target_es_es "Comprar comida y suministros"
     Comment "Interaction description on the general store counter"
    }
    CustomStringTableItem "{79EFB1B443677DA1}" {
     Id "LF-Shop_OpenDesc_Supermarket"
     Target_en_us "Buy groceries and drinks"
     Comment "Interaction description on the supermarket till (#3219)."
    }
    CustomStringTableItem "{B50F85FC9F4CC91E}" {
     Id "LF-Shop_Open_BackRoom"
     Target_en_us "Knock to buy a handgun"
     Target_es_es "Abrir mercado negro"
     Comment "Interaction name on the black-market shop"
    }
    CustomStringTableItem "{44F712AA62E8D66E}" {
     Id "LF-Shop_Open_Chandler"
     Target_en_us "Buy fishing tackle"
     Comment "Action prompt at the chandler (#2995)."
    }
    CustomStringTableItem "{FB0002094FEA5005}" {
     Id "LF-Shop_Open_Fence"
     Target_en_us "Talk to the fence"
     Comment "Interaction name on the fence NPC (#271)."
    }
    CustomStringTableItem "{7A090D0C714E7A03}" {
     Id "LF-Shop_Open_FishCounter"
     Target_en_us "Sell fish"
     Comment "Action prompt at the fish counter (#2995)."
    }
    CustomStringTableItem "{65F7FEC08E8AAFA4}" {
     Id "LF-Shop_Open_General"
     Target_en_us "Browse the store"
     Target_es_es "Abrir tienda"
     Comment "Interaction name on the general store counter"
    }
    CustomStringTableItem "{B62D96514AE04259}" {
     Id "LF-Shop_Open_GunCounter"
     Target_en_us "Buy a handgun"
     Comment "Action prompt at the gun counter (#988)."
    }
    CustomStringTableItem "{227F1753641762B8}" {
     Id "LF-Shop_Open_Supermarket"
     Target_en_us "Browse the shelves"
     Comment "Interaction name on the supermarket till (#3219)."
    }
    CustomStringTableItem "{BE3CD5847E4642EA}" {
     Id "LF-Shop_Order"
     Target_en_us "Order"
     Comment "Shop price list row button that puts the item on the order slip."
    }
    CustomStringTableItem "{00DDE66DC70CDDE2}" {
     Id "LF-Shop_Paid"
     Target_en_us "Paid %1. Thank you."
     Comment "Shop slip line after a purchase. %1 is the amount paid."
    }
    CustomStringTableItem "{F0B657CA9B23547B}" {
     Id "LF-Shop_Pay"
     Target_en_us "Pay cash"
     Comment "Shop till key before an item is picked."
    }
    CustomStringTableItem "{932451884B68684C}" {
     Id "LF-Shop_PayTotal"
     Target_en_us "Pay %1 cash"
     Comment "Shop till key. %1 is the order total."
    }
    CustomStringTableItem "{5F8B9B94FCD989FE}" {
     Id "LF-Shop_PickFirst"
     Target_en_us "Pick a line from the list first."
     Comment "Shop refusal on the client: Pay or Sell with no item on the slip."
    }
    CustomStringTableItem "{7788EC6B8586AE96}" {
     Id "LF-Shop_RefuseAlreadyOwned"
     Target_en_us "You already own one of these"
     Comment "Order slip line (#3295): an implement is bought once."
    }
    CustomStringTableItem "{601C97E84CB077AF}" {
     Id "LF-Shop_RefuseCash"
     Target_en_us "Not enough cash on you for that."
     Comment "Shop refusal: the buyer carries less cash than the total."
    }
    CustomStringTableItem "{7B1E5A0C00000001}" {
     Id "LF-Shop_RefuseClosed"
     Target_en_us "The back room is shut. {Police} have been through it. Come back in a few minutes."
     Comment "Shop refusal: a raided back room is shut for a while (#600)."
    }
    CustomStringTableItem "{3C541C5654D4DCEF}" {
     Id "LF-Shop_RefuseFar"
     Target_en_us "Step up to the counter."
     Comment "Shop refusal: the player is too far from the shop."
    }
    CustomStringTableItem "{49544D45EB25DEC6}" {
     Id "LF-Shop_RefuseNeedsJob"
     Target_en_us "Sold to %1 only."
     Comment "Order slip refusal at a gated outlet: %1 is the job the buyer must be in, e.g. 'Police' (#988)."
    }
    CustomStringTableItem "{8BCA5C98D5D6CB15}" {
     Id "LF-Shop_RefuseNeedsLicence"
     Target_en_us "Not without a %1."
     Comment "Order slip refusal at a gated outlet: %1 is the licence the buyer lacks, e.g. 'Firearm permit' (#988)."
    }
    CustomStringTableItem "{A2CCDA69FAA56F18}" {
     Id "LF-Shop_RefuseNeedsOrgRight"
     Target_en_us "Your organization rank lacks: %1."
     Comment "Order slip refusal at a gated outlet: %1 is the organisation right the buyer's rank must carry (#988)."
    }
    CustomStringTableItem "{739357983E4CECD0}" {
     Id "LF-Shop_RefuseNeedsRank"
     Target_en_us "Needs the rank %1."
     Comment "Order slip refusal at a gated outlet: %1 is the lowest rank title that may buy, e.g. 'Senior Officer' (#988)."
    }
    CustomStringTableItem "{B586E2861C625AE3}" {
     Id "LF-Shop_RefuseNoShop"
     Target_en_us "The till is closed."
     Comment "Shop refusal: the shop could not be found or the server has no buyer."
    }
    CustomStringTableItem "{61B8D820AD73FEDC}" {
     Id "LF-Shop_RefuseNotSold"
     Target_en_us "This counter does not trade that item."
     Comment "Shop refusal: the item is not sold here."
    }
    CustomStringTableItem "{69423E128B199BF7}" {
     Id "LF-Shop_RefuseNotTrusted"
     Target_en_us "I don't know you. Come back when you have made a name."
     Comment "Shop refusal: the fence has not seen enough of the buyer's work (#271)."
    }
    CustomStringTableItem "{6DC38164E7C5B1CF}" {
     Id "LF-Shop_RefuseNothing"
     Target_en_us "You are not carrying any of those."
     Comment "Shop refusal: a sale of an item the player does not carry."
    }
    CustomStringTableItem "{7998F3E07D68E505}" {
     Id "LF-Shop_RefuseOutOfStock"
     Target_en_us "Out of stock"
     Comment "Order slip: the store has fewer than you asked for (#202)."
    }
    CustomStringTableItem "{7E91491254C3EF1B}" {
     Id "LF-Shop_RefusePockets"
     Target_en_us "No room in your pockets."
     Comment "Shop refusal: the goods or the payout do not fit in the inventory; nothing changed hands."
    }
    CustomStringTableItem "{D4DD9D081F778020}" {
     Id "LF-Shop_RefuseQuantity"
     Target_en_us "Order between 1 and 99."
     Comment "Shop refusal: quantity outside the per-order limit."
    }
    CustomStringTableItem "{3A7C5E91D20B48F6}" {
     Id "LF-Shop_RefuseResident"
     Target_en_us "Not to you."
     Comment "Shop refusal: the resident clerk will not serve this buyer (#2911)."
    }
    CustomStringTableItem "{2D906536332C3F77}" {
     Id "LF-Shop_RefuseTooFast"
     Target_en_us "One order at a time. Give the till a moment."
     Comment "Shop refusal: too many orders in a short time (#693b rate limit)."
    }
    CustomStringTableItem "{9B4E445A7689D74C}" {
     Id "LF-Shop_RefuseUnfit"
     Target_en_us "The clerk won't hand that over to you in this state."
     Comment "Order slip line: a licensed line is not sold to a dead, downed or cuffed buyer."
    }
    CustomStringTableItem "{CD2FED1EA8EFF86B}" {
     Id "LF-Shop_RefuseUnpriced"
     Target_en_us "That item has no price here."
     Comment "Shop refusal: the item has no price."
    }
    CustomStringTableItem "{1CFEA3E02996B1FC}" {
     Id "LF-Shop_RefuseWanted"
     Target_en_us "You're wanted by the {police}. The counter won't sell you that."
     Comment "Order slip line: a licensed line (a firearm) is not sold to a wanted buyer."
    }
    CustomStringTableItem "{F612D2192DFC33C8}" {
     Id "LF-Shop_SaleFailed_Title"
     Target_en_us "Sale failed"
     Target_es_es "Venta fallida"
     Comment "Shop sell failure notification title (selling, not buying)"
    }
    CustomStringTableItem "{4D7E3AE3D3960088}" {
     Id "LF-Shop_SellBack"
     Target_en_us "Sell these"
     Comment "Shop till key: sell the slip's quantity of the picked item back to the counter."
    }
    CustomStringTableItem "{BAB816EED21FF256}" {
     Id "LF-Shop_SellTotal"
     Target_en_us "Sell for %1"
     Comment "Shop till key on a buyback line. %1 is what the counter pays."
    }
    CustomStringTableItem "{89D9F751DFF3D649}" {
     Id "LF-Shop_SlipEmpty"
     Target_en_us "Order slip: pick a line from the list."
     Comment "Shop order slip before an item is picked."
    }
    CustomStringTableItem "{1C832CFE28C35A01}" {
     Id "LF-Shop_SlipLine"
     Target_en_us "Order slip: %1 x %2"
     Comment "Shop order slip line. %1 is the quantity, %2 the item name."
    }
    CustomStringTableItem "{5A394611DC4124C6}" {
     Id "LF-Shop_SoldFor"
     Target_en_us "Sold for %1."
     Comment "Shop slip line after a sale. %1 is the amount received."
    }
    CustomStringTableItem "{0C99B82FA61A85FA}" {
     Id "LF-Shop_WePay"
     Target_en_us "We pay %1"
     Comment "Shop price list: price column of a line the counter only buys. %1 is what it pays for one."
    }
    CustomStringTableItem "{B385B4702F2997BF}" {
     Id "LF-Shop_Working"
     Target_en_us "At the till ..."
     Comment "Shop slip line while the server handles the order."
    }
    CustomStringTableItem "{9A17358E79382816}" {
     Id "LF-Shopkeeper_Name"
     Target_en_us "Shopkeeper"
     Comment "Speaker name of a store's shopkeeper in a conversation (#1143)."
    }
    CustomStringTableItem "{5F6ED016BC4C3C34}" {
     Id "LF-Shopkeeper_ShowGoods"
     Target_en_us "Show me what you've got."
     Comment "Dialogue reply to any NPC that keeps a shop: closes the talk and opens the price list."
    }
    CustomStringTableItem "{5B4C827A55F06D5F}" {
     Id "LF-Shopkeeper_ShowGoodsReply"
     Target_en_us "Take a look. Cash only."
     Comment "What a shopkeeper NPC says before the price list opens."
    }
    CustomStringTableItem "{D10D43C9BEB02467}" {
     Id "LF-Shopkeeper_Talk"
     Target_en_us "Talk to the shopkeeper"
     Comment "Action on a store's shopkeeper: opens the conversation (#1143)."
    }
    CustomStringTableItem "{126190E6CA344F29}" {
     Id "LF-ShoulderCarry_Action"
     Target_en_us "Hoist onto shoulder"
     Comment "F-key action name for shoulder carry (#3263)."
    }
    CustomStringTableItem "{4C9A65177C3AA068}" {
     Id "LF-ShoulderCarry_Denied"
     Target_en_us "Your hands are full."
     Comment "Greyed-out reason and toast when shoulder carry is refused (#3263)."
    }
    CustomStringTableItem "{6511ED0DDAE0AF1B}" {
     Id "LF-Sign_ChalkSample_Bait"
     Target_en_us "Fishing bait"
     Comment "Sample chalk board price row label."
    }
    CustomStringTableItem "{E4551DF35C444813}" {
     Id "LF-Sign_ChalkSample_Bread"
     Target_en_us "Bread"
     Comment "Sample chalk board price row label."
    }
    CustomStringTableItem "{2E0E075D4360F789}" {
     Id "LF-Sign_ChalkSample_Coffee"
     Target_en_us "Coffee"
     Comment "Sample chalk board price row label."
    }
    CustomStringTableItem "{AB08B102E0D6EDF8}" {
     Id "LF-Sign_ChalkSample_Fuel"
     Target_en_us "Jerry can of fuel"
     Comment "Sample chalk board price row label."
    }
    CustomStringTableItem "{A2E6C14B7D9035F8}" {
     Id "LF-Sign_ChalkSample_Next"
     Target_en_us "Walk on to the next cell."
     Comment "Next section of the sample chalk board's Read card (signs/next wants every sign to say what comes next)."
    }
    CustomStringTableItem "{F1E37CC41BF9295B}" {
     Id "LF-Sign_ChalkSample_Title"
     Target_en_us "Today's prices"
     Comment "Sample chalk board title (sign styles, #sign-overhaul): a generic price board."
    }
    CustomStringTableItem "{74C99019C7953F31}" {
     Id "LF-Sign_ChalkSample_What"
     Target_en_us "A chalk board with a title and a short price list."
     Comment "Read card of the sample chalk board."
    }
    CustomStringTableItem "{9E48B81765BAFEC3}" {
     Id "LF-Sign_ClinicShortage_Title"
     Target_en_us "Pharmacy closed"
     Comment "Sign title: the paper notice on the clinic door while the clinic is short (#4448)."
    }
    CustomStringTableItem "{62312EEA2BF57769}" {
     Id "LF-Sign_Close"
     Target_en_us "Put it back"
     Comment "Button that closes the sign card. The player is done reading the notice."
    }
  CustomStringTableItem "{FCEA5FDC41E5650D}" {
   Id "LF-Sign_Generic_Title"
   Target_en_us "Notice"
   Comment "Title of a sign card whose own text is not available (its pack is not loaded). Shown in the prompt as Read: Notice."
  }
    CustomStringTableItem "{116893FFB2C8AD3B}" {
     Id "LF-Sign_HouseForSale_Next"
     Target_en_us "Stand at the sign with the price in carried cash and choose Buy house."
     Comment "Read card of the house sale sign: what to do next (#3207)."
    }
    CustomStringTableItem "{BAEE50039356D187}" {
     Id "LF-Sign_HouseForSale_Title"
     Target_en_us "For sale"
     Comment "Core house sale sign face and Read title (#3207): a house nobody owns yet."
    }
    CustomStringTableItem "{8D334B4254D966D0}" {
     Id "LF-Sign_HouseForSale_What"
     Target_en_us "A house nobody owns yet. Use the sign to buy it with cash and get its key."
     Comment "Read card of the house sale sign (#3207)."
    }
    CustomStringTableItem "{4B91C7E2A05D3F18}" {
     Id "LF-Sign_HouseSold_Title"
     Target_en_us "Sold"
     Comment "Face and Read title of the house sale sign once somebody owns the house (sold-sign-board)."
    }
    CustomStringTableItem "{D3F60A8B1E742C95}" {
     Id "LF-Sign_HouseSold_What"
     Target_en_us "This house has an owner. It is no longer for sale."
     Comment "Read card of the house sale sign once the house is owned (sold-sign-board)."
    }
    CustomStringTableItem "{6A32B827033280BE}" {
     Id "LF-Sign_How"
     Target_en_us "How"
     Comment "Sign card section heading: how to use the place, with the action names."
    }
    CustomStringTableItem "{998F8A9D60C65734}" {
     Id "LF-Sign_Needs"
     Target_en_us "You need"
     Comment "Sign card section heading: items, faction, or people needed."
    }
    CustomStringTableItem "{B19428DE73DCD0A7}" {
     Id "LF-Sign_Next"
     Target_en_us "Next"
     Comment "Sign card section heading: where to go after this place."
    }
    CustomStringTableItem "{DFB2C6F5B97FC132}" {
     Id "LF-Sign_PlotForSale_Next"
     Target_en_us "Stand at the sign with the price in carried cash and choose Buy plot."
     Comment "Read card of the plot sign: what to do next (#2780)."
    }
    CustomStringTableItem "{BEBD6A10FB2E9747}" {
     Id "LF-Sign_PlotForSale_Title"
     Target_en_us "For sale"
     Comment "Core plot sign face and Read title (#2780): a plot nobody owns yet."
    }
    CustomStringTableItem "{3FF5200250B9B700}" {
     Id "LF-Sign_PlotForSale_What"
     Target_en_us "A plot of land nobody owns yet. Use the sign to buy it with cash."
     Comment "Read card of the plot sign (#2780)."
    }
    CustomStringTableItem "{2A7E5C19F8B04D63}" {
     Id "LF-Sign_PlotSold_Title"
     Target_en_us "Sold"
     Comment "Face and Read title of the plot sign once somebody owns the plot (sold-sign-board)."
    }
    CustomStringTableItem "{86C1F2D40B9A7E35}" {
     Id "LF-Sign_PlotSold_What"
     Target_en_us "This plot of land has an owner. It is no longer for sale."
     Comment "Read card of the plot sign once the plot is owned (sold-sign-board)."
    }
    CustomStringTableItem "{C71FE54AE5989467}" {
     Id "LF-Sign_Read"
     Target_en_us "Read: %1"
     Comment "Interaction prompt on a sign, board, or notice. %1 is the sign title, e.g. Read: Mining area."
    }
    CustomStringTableItem "{B4AC01A970C24DB6}" {
     Id "LF-Sign_Rules"
     Target_en_us "Rules"
     Comment "Sign card section heading: cooldowns, wanted level, other rules."
    }
    CustomStringTableItem "{F5CEEA586CBB4B8F}" {
     Id "LF-Sign_ShiftBoard_Next"
     Target_en_us "Keep working this site to climb the board."
     Comment "Sign catalog next text of the posted shift board's Read card (sign id shift_board): what to do after reading it."
    }
    CustomStringTableItem "{3EFD6AD0426C7BB2}" {
     Id "LF-Sign_ShiftBoard_Title"
     Target_en_us "Shift board"
     Comment "Title painted on the posted shift board at a work site (polish of #4713) and shown in the prompt as Read: Shift board."
    }
    CustomStringTableItem "{BF1EE8DF0BD254A0}" {
     Id "LF-Sign_ShiftBoard_What"
     Target_en_us "The week's top five workers at this site by the trade XP they earned here. Read the board to see where you stand."
     Comment "Sign card for the posted shift board, What section (polish of #4713)."
    }
    CustomStringTableItem "{B71E5C09A4D23F68}" {
     Id "LF-Sign_TaxiStand_Next"
     Target_en_us "Take taxi work here, then drive each fare where it wants to go."
     Comment "Sign catalog 'next' text of the taxi stand's Read card (sign id taxi_stand): what to do after reading it."
    }
    CustomStringTableItem "{6EAD44E4FC5E58E9}" {
     Id "LF-Sign_TaxiStand_Title"
     Target_en_us "Taxi stand"
     Comment "Title printed on the taxi stand's pole plate (LF_TaxiStand prefab, sign id taxi_stand); the Read card shows it."
    }
    CustomStringTableItem "{C893332322755556}" {
     Id "LF-Sign_TaxiStand_What"
     Target_en_us "The taxi stand: sign on for taxi work and pick up fares who need a ride."
     Comment "Sign catalog 'what' text of the taxi stand's Read card (sign id taxi_stand)."
    }
    CustomStringTableItem "{84F64B2018E1B414}" {
     Id "LF-Sign_What"
     Target_en_us "What it is"
     Comment "Sign card section heading: what this place is."
    }
    CustomStringTableItem "{D9DA509A13EDB5EA}" {
     Id "LF-Siren_AmbulanceOff"
     Target_en_us "Siren: off"
     Comment "Action on the ambulance's light switch: stop the siren and the light bar (#923)."
    }
    CustomStringTableItem "{53FFDC4A55C11ED4}" {
     Id "LF-Siren_AmbulanceOn"
     Target_en_us "Siren: on"
     Comment "Action on the ambulance's light switch: start the hi-lo siren and the light bar (#923)."
    }
    CustomStringTableItem "{7A31C0DE5A1E0001}" {
     Id "LF-Siren_NeedMedic"
     Target_en_us "Clock in to use the siren"
     Comment "Greyed reason on the ambulance siren action when the driver is not an on-duty medic."
    }
    CustomStringTableItem "{7A31C0DE5A1E0002}" {
     Id "LF-Siren_NeedPolice"
     Target_en_us "Clock in to use the siren"
     Comment "Greyed reason on a police siren action when the driver is not an on-duty officer."
    }
    CustomStringTableItem "{5EBE8CF2BBC1E139}" {
     Id "LF-Sit_Action"
     Target_en_us "Sit"
     Comment "F-key action on a chair or bench: sit down on it (#982)."
    }
    CustomStringTableItem "{41F7E124C0FE4E7B}" {
     Id "LF-Sit_Denied"
     Target_en_us "You cannot sit down now"
     Comment "Greyed-out reason on Sit when the body cannot sit: dead, stunned, in a vehicle or playing a gesture (#982)."
    }
    CustomStringTableItem "{48019BCFB08527D2}" {
     Id "LF-Sit_Occupied"
     Target_en_us "Someone is sitting here"
     Comment "Greyed-out reason on Sit when another player sits on that place (#982)."
    }
    CustomStringTableItem "{A9D554987763FD86}" {
     Id "LF-Sit_StandUp"
     Target_en_us "Stand up"
     Comment "F-key action on the seat you are sitting on: stand up (#982)."
    }
    CustomStringTableItem "{0008CC6249A5CFEE}" {
     Id "LF-SiteAlert_Captured"
     Target_en_us "Your organization captured a contested site."
     Comment "Toast to the capturing organisation's members when a site changes hands (#2686)."
    }
    CustomStringTableItem "{C4975E59132BD691}" {
     Id "LF-SiteAlert_Contested"
     Target_en_us "Your held ground is being contested!"
     Comment "Toast to every online member of the holding organisation when a site becomes contested (#2686)."
    }
    CustomStringTableItem "{4A0872A212CC8663}" {
     Id "LF-SiteAlert_Lost"
     Target_en_us "Your organization lost a contested site."
     Comment "Toast to the old holder's members when a site is captured (#2686)."
    }
    CustomStringTableItem "{238A8F0864906DC7}" {
     Id "LF-SiteAlert_Title"
     Target_en_us "Gang war"
     Comment "Toast title for a site alert to an organisation (#2686)."
    }
    CustomStringTableItem "{FF6CC0CF9F225E83}" {
     Id "LF-SiteCapture_Capturing"
     Target_en_us "%1 capturing: %2%"
     Comment "Capture bar progress line. %1 = the challenging organisation, %2 = percent (#2686)."
    }
    CustomStringTableItem "{976106A58BF13557}" {
     Id "LF-SiteCapture_Fallback"
     Target_en_us "Contested ground"
     Comment "Capture bar title when the pack gives the site no LF-SiteName_<hideout id> row (#2686)."
    }
    CustomStringTableItem "{B61D9A845B840433}" {
     Id "LF-SiteCapture_HeldBy"
     Target_en_us "Held by %1"
     Comment "Capture bar holder line. %1 = the holding organisation (#2686)."
    }
    CustomStringTableItem "{B04D9B6C54D7F8AA}" {
     Id "LF-SiteCapture_Idle"
     Target_en_us "Nobody is capturing"
     Comment "Capture bar progress line while no challenger has progress (#2686)."
    }
    CustomStringTableItem "{C3855F9D427A1467}" {
     Id "LF-SiteCapture_Unclaimed"
     Target_en_us "Unclaimed"
     Comment "Capture bar holder line when nobody holds the site (#2686)."
    }
    CustomStringTableItem "{E90742664100610A}" {
     Id "LF-SiteFlag_Unclaimed"
     Target_en_us "Unclaimed"
     Comment "Painted board on a contested site's flag pole when no organisation holds the site."
    }
    CustomStringTableItem "{970E45E130DA2448}" {
     Id "LF-SiteStrongbox_Empty"
     Target_en_us "The strongbox is empty."
     Comment "Toast: the holder opens the site strongbox and no rent is banked (#2684)."
    }
    CustomStringTableItem "{E97E258CA4D33FE7}" {
     Id "LF-SiteStrongbox_NoRight"
     Target_en_us "Your rank cannot take the rent."
     Comment "Toast: a member of the holding organisation whose rank lacks the withdraw right (#2684)."
    }
    CustomStringTableItem "{CEEAC310316FCF52}" {
     Id "LF-SiteStrongbox_NoRoom"
     Target_en_us "No room to carry the cash. The rent stays in the strongbox."
     Comment "Toast: the cash did not fit in the player inventory; the bank is unchanged (#2684)."
    }
    CustomStringTableItem "{B6BB27B3E34F8BD4}" {
     Id "LF-SiteStrongbox_NotHolder"
     Target_en_us "Only the organization that holds this site can open the strongbox."
     Comment "Toast: a non-member or another organisation tries the site strongbox (#2684)."
    }
    CustomStringTableItem "{15D45ACB87B1F80A}" {
     Id "LF-SiteStrongbox_Take"
     Target_en_us "Take the rent"
     Comment "Action name on a site strongbox (#2684)."
    }
    CustomStringTableItem "{FB8C42542BB9F188}" {
     Id "LF-SiteStrongbox_Taken"
     Target_en_us "You take %1 in rent from the strongbox."
     Comment "Toast: the holder took the banked rent as cash. %1 = the amount (#2684)."
    }
    CustomStringTableItem "{41D611C1A02BCA46}" {
     Id "LF-SiteStrongbox_Title"
     Target_en_us "Strongbox"
     Comment "Toast title for the site strongbox (#2684)."
    }
    CustomStringTableItem "{134044FE4B970260}" {
     Id "LF-SpawnHub_Close"
     Target_en_us "Close"
     Comment "Travel list close button."
    }
    CustomStringTableItem "{64C91C9876BC49F4}" {
     Id "LF-SpawnHub_Go"
     Target_en_us "Go"
     Comment "Travel list row button."
    }
    CustomStringTableItem "{D2F012D65C8132A6}" {
     Id "LF-SpawnHub_Hint"
     Target_en_us "Pick an area to be taken there. Playtest only."
     Comment "Travel list hint line."
    }
    CustomStringTableItem "{BEEAD653C82FCDCE}" {
     Id "LF-SpawnHub_Open"
     Target_en_us "Travel to a playtest area"
     Comment "Globe action name."
    }
    CustomStringTableItem "{D25E9BC45F9B3F3E}" {
     Id "LF-SpawnHub_Pt_annex"
     Target_en_us "Test annex"
     Comment "Travel list row: annex."
    }
    CustomStringTableItem "{8AA2600D1543035B}" {
     Id "LF-SpawnHub_Pt_bank"
     Target_en_us "Bank"
     Comment "Travel list row: bank."
    }
    CustomStringTableItem "{4AF36AD0E0DCF596}" {
     Id "LF-SpawnHub_Pt_car_lot"
     Target_en_us "Car lot"
     Comment "Travel list row: car_lot."
    }
    CustomStringTableItem "{B20B273E74BFB5FC}" {
     Id "LF-SpawnHub_Pt_cash_van"
     Target_en_us "Cash van stop"
     Comment "Travel list row: cash_van."
    }
    CustomStringTableItem "{4933783F01581685}" {
     Id "LF-SpawnHub_Pt_chop_shop"
     Target_en_us "Chop shop"
     Comment "Travel list row: chop_shop."
    }
    CustomStringTableItem "{63BC79F23121554D}" {
     Id "LF-SpawnHub_Pt_farm"
     Target_en_us "Farm"
     Comment "Travel list row: farm."
    }
    CustomStringTableItem "{88BCEC3B8F0B96E6}" {
     Id "LF-SpawnHub_Pt_fence"
     Target_en_us "Fence (Vince)"
     Comment "Travel list row: fence."
    }
    CustomStringTableItem "{FB4A0D3B97262A9C}" {
     Id "LF-SpawnHub_Pt_fuel_station"
     Target_en_us "Fuel station"
     Comment "Travel list row: fuel_station."
    }
    CustomStringTableItem "{69B85ADAEFBDBB9C}" {
     Id "LF-SpawnHub_Pt_garage"
     Target_en_us "Garage"
     Comment "Travel list row: garage."
    }
    CustomStringTableItem "{CB3B602C123EDB3A}" {
     Id "LF-SpawnHub_Pt_general_store"
     Target_en_us "General store"
     Comment "Travel list row: general_store."
    }
    CustomStringTableItem "{180961ECD408F286}" {
     Id "LF-SpawnHub_Pt_hideout"
     Target_en_us "Hideout yard"
     Comment "Travel list row: hideout."
    }
    CustomStringTableItem "{DFCE41C7B190AA27}" {
     Id "LF-SpawnHub_Pt_hospital"
     Target_en_us "Hospital"
     Comment "Travel list row: hospital."
    }
    CustomStringTableItem "{B0C61257E5FB48B5}" {
     Id "LF-SpawnHub_Pt_impound"
     Target_en_us "Impound lot"
     Comment "Travel list row: impound."
    }
    CustomStringTableItem "{B4232BB7C4308A82}" {
     Id "LF-SpawnHub_Pt_jail"
     Target_en_us "Jail"
     Comment "Travel list row: jail."
    }
    CustomStringTableItem "{3502F9E9ED2D4D43}" {
     Id "LF-SpawnHub_Pt_lockup"
     Target_en_us "Lockpick lock-up"
     Comment "Travel list row: lockup."
    }
    CustomStringTableItem "{70A38A6809640B2F}" {
     Id "LF-SpawnHub_Pt_market"
     Target_en_us "Market traders"
     Comment "Travel list row: market."
    }
    CustomStringTableItem "{9ECE67180191DEF9}" {
     Id "LF-SpawnHub_Pt_mining"
     Target_en_us "Mining yard"
     Comment "Travel list row: mining."
    }
    CustomStringTableItem "{ACA4B36A252C8E65}" {
     Id "LF-SpawnHub_Pt_moonshine"
     Target_en_us "Moonshine camp"
     Comment "Travel list row: moonshine."
    }
    CustomStringTableItem "{77D8780E1D048421}" {
     Id "LF-SpawnHub_Pt_orchard"
     Target_en_us "Apple orchard"
     Comment "Travel list row: orchard."
    }
    CustomStringTableItem "{6D24484296F44B77}" {
     Id "LF-SpawnHub_Pt_org_registry"
     Target_en_us "Organization registry"
     Comment "Travel list row: org_registry."
    }
    CustomStringTableItem "{B6586A394D67B4AF}" {
     Id "LF-SpawnHub_Pt_police"
     Target_en_us "Police station"
     Comment "Travel list row: police."
    }
    CustomStringTableItem "{F7342C42AF67BCAA}" {
     Id "LF-SpawnHub_Pt_post_office"
     Target_en_us "Post office and jobs"
     Comment "Travel list row: post_office."
    }
    CustomStringTableItem "{BA0368C4FE08E132}" {
     Id "LF-SpawnHub_Pt_quarry"
     Target_en_us "Quarry (lawless)"
     Comment "Travel list row: quarry."
    }
    CustomStringTableItem "{450BE9C116C859BD}" {
     Id "LF-SpawnHub_Pt_spawn"
     Target_en_us "Spawn square"
     Comment "Travel list row: spawn."
    }
    CustomStringTableItem "{EE3AD272B57D4B3B}" {
     Id "LF-SpawnHub_ResetSure"
     Target_en_us "Sure? Resets for everyone"
     Comment "Travel globe reset row after the first press."
    }
    CustomStringTableItem "{A3B9C629E6434881}" {
     Id "LF-SpawnHub_ResetWorld"
     Target_en_us "Reset the world"
     Comment "Travel globe list row: puts the playtest world back as it was for everyone."
    }
    CustomStringTableItem "{01B9D43100AC37E2}" {
     Id "LF-SpawnHub_Title"
     Target_en_us "Playtest travel"
     Comment "Travel list title (spawn table globe)."
    }
    CustomStringTableItem "{C46B3F43B4495744}" {
     Id "LF-SpawnHub_TooFar"
     Target_en_us "Stand at the travel globe to travel."
     Comment "Spawn hub refusal: the server found no travel globe next to the player (#693b)."
    }
    CustomStringTableItem "{19BD6378F5F924A0}" {
     Id "LF-SpikeStrip_Cap"
     Target_en_us "You already have your limit of spike strips down. Pick one up first."
     Comment "Toast: the officer tried to lay another spike strip while the configured number of their own is still deployed (#2416)."
    }
    CustomStringTableItem "{C9A02C030858FF1F}" {
     Id "LF-SpikeStrip_Deploy"
     Target_en_us "Deploy spike strip"
     Comment "Action on the folded strip: lays it across the road in front of the officer."
    }
    CustomStringTableItem "{633CC534006B5907}" {
     Id "LF-SpikeStrip_Description"
     Target_en_us "Folding tire-deflation strip. Drop it at the roadside and deploy it across the road; a tire that crosses it goes flat. On-duty police only."
     Comment "Inventory description of the folded police stinger."
    }
    CustomStringTableItem "{70EF9362784F2066}" {
     Id "LF-SpikeStrip_Name"
     Target_en_us "Spike strip"
     Comment "Inventory name of the folded police stinger (#646)."
    }
    CustomStringTableItem "{A09B30F6281E5E2E}" {
     Id "LF-SpikeStrip_NoGround"
     Target_en_us "There is no flat ground in front of you for the strip."
     Comment "Toast when the ground ahead rises or falls too much along the strip (a kerb, a car, a slope)."
    }
    CustomStringTableItem "{4CC146840395573D}" {
     Id "LF-SpikeStrip_PickUp"
     Target_en_us "Pick up spike strip"
     Comment "Action on a deployed strip: folds it back into the officer inventory."
    }
    CustomStringTableItem "{C73AA5808AE4125F}" {
     Id "LF-SpikeStrip_PoliceOnly"
     Target_en_us "Only an on-duty officer can handle the spike strip."
     Comment "Toast and greyed-out reason when someone who is not on-duty police tries the strip."
    }
    CustomStringTableItem "{E3A1B2C4D5F60738}" {
     Id "LF-Stall_AlreadyRented"
     Target_en_us "This stall is taken, or you already rent one today."
     Comment "Toast: stall rent refused, the stall is rented or the player has another; nothing was taken (#2925)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60742}" {
     Id "LF-Stall_Bought"
     Target_en_us "You bought an item from the stall for %1."
     Comment "Toast to the buyer: cash paid and the item is in the inventory (#2926). %1 = the price paid."
    }
    CustomStringTableItem "{E3A1B2C4D5F60740}" {
     Id "LF-Stall_Buy"
     Target_en_us "Buy from the stall"
     Comment "Action on a rented market stall: buys the cheapest listing (#2926)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60741}" {
     Id "LF-Stall_BuyDesc"
     Target_en_us "Buy the cheapest item on this stall."
     Comment "Description of the buy action on a market stall (#2926)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60744}" {
     Id "LF-Stall_BuyerFull"
     Target_en_us "You have no room to carry it. Nothing was taken."
     Comment "Toast: buy refused, the item did not fit the buyer's inventory; cash refunded (#2926)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60735}" {
     Id "LF-Stall_Closed"
     Target_en_us "The market is closed. Stalls rent on market day."
     Comment "Cannot-rent reason and toast outside market hours (#2925)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60737}" {
     Id "LF-Stall_NoCash"
     Target_en_us "You do not carry enough cash for the pitch fee."
     Comment "Toast: stall rent refused, short of cash; nothing was taken (#2925)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60746}" {
     Id "LF-Stall_NoCashBuy"
     Target_en_us "You do not carry enough cash for that."
     Comment "Toast: buy refused, short of cash; nothing was taken (#2926)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60743}" {
     Id "LF-Stall_NothingListed"
     Target_en_us "This stall has nothing for sale right now."
     Comment "Toast: buy refused, no open listing; nothing moved (#2926)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60747}" {
     Id "LF-Stall_OwnStall"
     Target_en_us "You cannot buy from your own stall."
     Comment "Toast: the holder tried to buy their own listing (#2926)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60731}" {
     Id "LF-Stall_Rent"
     Target_en_us "Rent stall (%1)"
     Comment "Action on a free market stall (#2925). %1 = the formatted pitch fee."
    }
    CustomStringTableItem "{E3A1B2C4D5F60732}" {
     Id "LF-Stall_RentDesc"
     Target_en_us "Rent this stall for today's market."
     Comment "Description of the rent action on a market stall (#2925)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60736}" {
     Id "LF-Stall_Rented"
     Target_en_us "You rented a stall for today's market."
     Comment "Toast: the pitch fee was paid and the stall is yours until the market closes (#2925)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60748}" {
     Id "LF-Stall_Sold"
     Target_en_us "You sold an item at your stall and were paid %1."
     Comment "Toast to the holder: a sale settled and the cash is in their pockets (#2926). %1 = the amount paid after the market's cut."
    }
    CustomStringTableItem "{E3A1B2C4D5F60733}" {
     Id "LF-Stall_Taken"
     Target_en_us "Stall of %1"
     Comment "Prompt on a rented market stall (#2925). %1 = the holder's name."
    }
    CustomStringTableItem "{E3A1B2C4D5F60730}" {
     Id "LF-Stall_Title"
     Target_en_us "Market stall"
     Comment "Toast title for a market stall rent (#2925)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60745}" {
     Id "LF-Stall_Unavailable"
     Target_en_us "That item is no longer available. Nothing was taken."
     Comment "Toast: buy refused, the holder no longer has it or is away; cash refunded (#2926)."
    }
    CustomStringTableItem "{E3A1B2C4D5F60734}" {
     Id "LF-Stall_Unnamed"
     Target_en_us "a trader"
     Comment "Holder label on a rented stall whose holder has no name (#2925)."
    }
    CustomStringTableItem "{044EF7358D99C82D}" {
     Id "LF-StashContainer_Closed"
     Target_en_us "The container won't open"
     Comment "Refusal on the short container: misconfigured or no player (#4195)."
    }
    CustomStringTableItem "{DC4609ADE4E93025}" {
     Id "LF-StashContainer_Empty"
     Target_en_us "Empty. Come back later"
     Comment "Refusal on the short container: already taken, the refill timer is running (#4195)."
    }
    CustomStringTableItem "{DA310007BD3F516C}" {
     Id "LF-StashContainer_NeedKey"
     Target_en_us "Locked. You need the clerk's key"
     Comment "Refusal on the short container: the player carries no key from the tally clerk (#4195)."
    }
    CustomStringTableItem "{BA110B9CEFF14792}" {
     Id "LF-StashContainer_Open"
     Target_en_us "Open the container"
     Comment "Action: open the short container with the tally clerk's key and take the stock (#4195)."
    }
    CustomStringTableItem "{C6CD1F2F0A61F3B5}" {
     Id "LF-Stolen_Already"
     Target_en_us "The {police} already have this one"
     Comment "Toast: report refused (#414)."
    }
    CustomStringTableItem "{7D85335FFB1F2738}" {
     Id "LF-Stolen_BoardLine"
     Target_en_us "STOLEN CAR: %2, plate %1, reported at %3"
     Comment "Wanted board line: a reported stolen car (#414)."
    }
    CustomStringTableItem "{54D39129C638398D}" {
     Id "LF-Stolen_CheckAction"
     Target_en_us "Check the plate"
     Comment "Action: an officer checks the plate (#414)."
    }
    CustomStringTableItem "{7B3EF21861B9946F}" {
     Id "LF-Stolen_CheckTitle"
     Target_en_us "Plate check"
     Comment "Toast title: officer plate check and recovery (#414)."
    }
    CustomStringTableItem "{B07C0014F4165300}" {
     Id "LF-Stolen_NoVehicle"
     Target_en_us "That is not a registered car"
     Comment "Toast: report refused (#414)."
    }
    CustomStringTableItem "{74B600E93F163F6F}" {
     Id "LF-Stolen_NotOwner"
     Target_en_us "That is not your car"
     Comment "Toast: report refused (#414)."
    }
    CustomStringTableItem "{4E2A1DBE21617A8A}" {
     Id "LF-Stolen_NotStolen"
     Target_en_us "That car was not stolen"
     Comment "Toast: report refused (#414)."
    }
    CustomStringTableItem "{D5ACFFFEB37116C0}" {
     Id "LF-Stolen_PlateClear"
     Target_en_us "Plate %1: registered, no report."
     Comment "Toast: plate check, clean (#414)."
    }
    CustomStringTableItem "{F9299F02744818C6}" {
     Id "LF-Stolen_PlateStolen"
     Target_en_us "Plate %1: STOLEN. Reported by the owner. Arrest case."
     Comment "Toast: plate check on a reported car (#414)."
    }
    CustomStringTableItem "{5C0613E010C6A887}" {
     Id "LF-Stolen_PlateUnknown"
     Target_en_us "Plate: not on any register."
     Comment "Toast: plate check, unknown plate (#414)."
    }
    CustomStringTableItem "{81EB652362FAF78A}" {
     Id "LF-Stolen_RecoverAction"
     Target_en_us "Recover stolen vehicle"
     Comment "Action: an officer returns a reported car to its owner's garage (#414)."
    }
    CustomStringTableItem "{B809D325187A4334}" {
     Id "LF-Stolen_RecoverNone"
     Target_en_us "No stolen report on this car"
     Comment "Toast: recover refused (#414)."
    }
    CustomStringTableItem "{BEEF3229C82D306C}" {
     Id "LF-Stolen_RecoverOccupied"
     Target_en_us "Get the driver out first"
     Comment "Toast: recover refused (#414)."
    }
    CustomStringTableItem "{F215C82571D6753A}" {
     Id "LF-Stolen_Recovered"
     Target_en_us "Plate %1 recovered: back in the owner's garage."
     Comment "Toast: recovered (#414)."
    }
    CustomStringTableItem "{07590CA5189B8EC3}" {
     Id "LF-Stolen_ReportAction"
     Target_en_us "Report the car stolen"
     Comment "Action: the owner reports a stolen car (#414)."
    }
    CustomStringTableItem "{45C2C41BF9850640}" {
     Id "LF-Stolen_Reported"
     Target_en_us "Reported. The {police} have the plate."
     Comment "Toast: report filed (#414)."
    }
    CustomStringTableItem "{C04E7ECCE5EA1009}" {
     Id "LF-Stolen_Title"
     Target_en_us "Stolen car"
     Comment "Toast title (#414)."
    }
    CustomStringTableItem "{23BA6EE5AB3BA9D3}" {
     Id "LF-Store_ClerkThief"
     Target_en_us "Stop! You did not pay for that!"
     Comment "Key a store's staff say a line for when a shopper walks out with unpaid goods; the dialogue's own line is the caption (#1143)."
    }
    CustomStringTableItem "{E76A90141796C076}" {
     Id "LF-Store_NoStore"
     Target_en_us "This counter is not a till."
     Comment "Toast: the till action ran on something that is not a store (#202)."
    }
    CustomStringTableItem "{6A109C7952B78879}" {
     Id "LF-Store_NotEnough"
     Target_en_us "You owe {currency:sym}%1 and do not have that much cash."
     Comment "Toast: not enough cash at the till (#202). %1 is the total owed."
    }
    CustomStringTableItem "{2D6C14FA64678072}" {
     Id "LF-Store_NothingToPay"
     Target_en_us "You are not carrying anything from the shelves."
     Comment "Toast: the till has nothing to charge (#202)."
    }
    CustomStringTableItem "{2FE09D890B43FD73}" {
     Id "LF-Store_Paid"
     Target_en_us "Paid {currency:sym}%1. Thank you, here is your receipt."
     Comment "Toast: the till took the cash and printed a receipt (#202). %1 is the total."
    }
    CustomStringTableItem "{A674178BB9E4A612}" {
     Id "LF-Store_Pay"
     Target_en_us "Pay for goods"
     Comment "Counter action: pay for the goods you carry from the shelves (#202)."
    }
    CustomStringTableItem "{010DA8BE735E4DFA}" {
     Id "LF-Store_PayFailed"
     Target_en_us "The till could not take the cash. Nothing was charged."
     Comment "Toast: the cash removal failed and was rolled back (#202)."
    }
    CustomStringTableItem "{76DB00E0F82A1123}" {
     Id "LF-Store_ReceiptBlank"
     Target_en_us "The ink has faded. The receipt is blank."
     Comment "Toast: a receipt carried over a restart reads blank (#202)."
    }
    CustomStringTableItem "{39DB2A87B5A357EA}" {
     Id "LF-Store_ReceiptItem"
     Target_en_us "Receipt"
     Comment "Item name: till receipt (#202)."
    }
    CustomStringTableItem "{E5A5E7530BC7A76E}" {
     Id "LF-Store_ReceiptItemDesc"
     Target_en_us "Proof that the goods were paid for at the till."
     Comment "Item description: till receipt (#202)."
    }
    CustomStringTableItem "{CD891C3C3B261FAD}" {
     Id "LF-Store_ReceiptRead"
     Target_en_us "Read receipt"
     Comment "Item action: read a receipt (#202)."
    }
    CustomStringTableItem "{38DBA55BD5173210}" {
     Id "LF-Store_ReceiptText"
     Target_en_us "Receipt %1"
     Comment "Toast: what the receipt says (#202). %1 is number, items, total."
    }
    CustomStringTableItem "{2D524086F58B8EF6}" {
     Id "LF-Store_ReceiptTitle"
     Target_en_us "Receipt"
     Comment "Toast title of a read receipt (#202)."
    }
    CustomStringTableItem "{DBC751DB441FD67F}" {
     Id "LF-Store_Thief"
     Target_en_us "You walked out without paying. The alarm is going off and the shopkeeper knows your face."
     Comment "Toast: you left a store with unpaid goods (#202)."
    }
    CustomStringTableItem "{F68211C7B6D815DE}" {
     Id "LF-Store_Title"
     Target_en_us "Store"
     Comment "Toast title for the till and the shoplifting alarm (#202)."
    }
    CustomStringTableItem "{005CFE757CF202AB}" {
     Id "LF-Store_UnpaidRestored"
     Target_en_us "You still carry goods you have not paid for. Go back to the store and pay within %1 minute(s), or it counts as shoplifting."
     Comment "Toast at spawn: unpaid store goods came back after a restart or a logout (#479). %1 = minutes left."
    }
    CustomStringTableItem "{64BA75A2F63B70CD}" {
     Id "LF-Systems_Off"
     Target_en_us "That is turned off on this server."
     Comment "Caption or toast when a player reaches something whose system the server owner turned off: a till robbery begun before the switch, a farm plot (#960)."
    }
    CustomStringTableItem "{D4EF4181AF1095B4}" {
     Id "LF-Term_Currency"
     Target_en_us "dollar"
     Target_de_de "Dollar"
     Target_fr_fr "dollar"
     Target_it_it "dollaro"
     Target_es_es "dólar"
     Target_pt_pt "dólar"
     Target_pt_br "dólar"
     Comment "Term "currency" (#958): the money unit, singular."
    }
    CustomStringTableItem "{4C9D9FCD0F57F53F}" {
     Id "LF-Term_Currency_Plural"
     Target_en_us "dollars"
     Target_de_de "Dollar"
     Target_fr_fr "dollars"
     Target_it_it "dollari"
     Target_es_es "dólares"
     Target_pt_pt "dólares"
     Target_pt_br "dólares"
     Comment "Term "currency" (#958): the money unit, plural, {currency:pl}."
    }
    CustomStringTableItem "{F8FF62CD88A6A2E3}" {
     Id "LF-Term_Currency_Sym"
     Target_en_us "$"
     Target_de_de "$"
     Target_fr_fr "$"
     Target_it_it "$"
     Target_es_es "$"
     Target_pt_pt "$"
     Target_pt_br "$"
     Comment "Term "currency" (#958): the symbol that goes before an amount, {currency:sym}."
    }
    CustomStringTableItem "{B543BF50A77A5A7C}" {
     Id "LF-Term_Fine"
     Target_en_us "fine"
     Target_de_de "Bußgeld"
     Target_fr_fr "amende"
     Target_it_it "multa"
     Target_es_es "multa"
     Target_pt_pt "multa"
     Target_pt_br "multa"
     Comment "Term "fine" (#1136): a police fine as a sentence names it. {fine} or {Fine}."
    }
    CustomStringTableItem "{CA4254F348EBC44B}" {
     Id "LF-Term_Fine_Plural"
     Target_en_us "fines"
     Target_de_de "Bußgelder"
     Target_fr_fr "amendes"
     Target_it_it "multe"
     Target_es_es "multas"
     Target_pt_pt "multas"
     Target_pt_br "multas"
     Comment "Term "fine", plural form: {fine:pl}."
    }
    CustomStringTableItem "{B07D4BA4D1FF679B}" {
     Id "LF-Term_Job"
     Target_en_us "job"
     Target_de_de "Beruf"
     Target_fr_fr "métier"
     Target_it_it "lavoro"
     Target_es_es "trabajo"
     Target_pt_pt "trabalho"
     Target_pt_br "trabalho"
     Comment "Term job singular (#1136): {job}. The named forms ({job:miner}) are the jobs' own name keys."
    }
    CustomStringTableItem "{8BF1E0FB4113B1DE}" {
     Id "LF-Term_Job_Plural"
     Target_en_us "jobs"
     Target_de_de "Berufe"
     Target_fr_fr "métiers"
     Target_it_it "lavori"
     Target_es_es "trabajos"
     Target_pt_pt "trabalhos"
     Target_pt_br "trabalhos"
     Comment "Term job, plural form: {job:pl}."
    }
    CustomStringTableItem "{04400B2027C380FF}" {
     Id "LF-Term_Medic"
     Target_en_us "medic"
     Target_de_de "Sanitäter"
     Target_fr_fr "médecin"
     Target_it_it "medico"
     Target_es_es "médico"
     Target_pt_pt "médico"
     Target_pt_br "médico"
     Comment "Term "medic" (#958): singular."
    }
    CustomStringTableItem "{D16A258D77A04F85}" {
     Id "LF-Term_Medic_Plural"
     Target_en_us "medics"
     Target_de_de "Sanitäter"
     Target_fr_fr "médecins"
     Target_it_it "medici"
     Target_es_es "médicos"
     Target_pt_pt "médicos"
     Target_pt_br "médicos"
     Comment "Term "medic" (#958): plural, {medic:pl}."
    }
    CustomStringTableItem "{EFA49FCF4C181DD3}" {
     Id "LF-Term_Org"
     Target_en_us "organization"
     Target_de_de "Organisation"
     Target_fr_fr "organisation"
     Target_it_it "organizzazione"
     Target_es_es "organización"
     Target_pt_pt "organização"
     Target_pt_br "organização"
     Comment "Term "org" (#1136): a player organization as a sentence names it. Core text writes {org} or {Org}; a pack or the owner renames it (Gang, Crew)."
    }
    CustomStringTableItem "{857D8BAEB3AA5709}" {
     Id "LF-Term_Org_Plural"
     Target_en_us "organizations"
     Target_de_de "Organisationen"
     Target_fr_fr "organisations"
     Target_it_it "organizzazioni"
     Target_es_es "organizaciones"
     Target_pt_pt "organizações"
     Target_pt_br "organizações"
     Comment "Term "org", plural form: {org:pl}."
    }
    CustomStringTableItem "{67F7742CC0C6E47C}" {
     Id "LF-Term_Police"
     Target_en_us "police"
     Target_de_de "Polizei"
     Target_fr_fr "police"
     Target_it_it "polizia"
     Target_es_es "policía"
     Target_pt_pt "polícia"
     Target_pt_br "polícia"
     Comment "Term "police" (#958): the police service as a sentence names it. Core text writes {police} or {Police}; a pack or the owner renames it."
    }
    CustomStringTableItem "{7653CEA2A3D01C1D}" {
     Id "LF-Term_Town"
     Target_en_us "town"
     Target_de_de "Stadt"
     Target_fr_fr "ville"
     Target_it_it "città"
     Target_es_es "ciudad"
     Target_pt_pt "cidade"
     Target_pt_br "cidade"
     Comment "Term "town" (#958): what the settlement is called. Core text writes {town} or {Town}."
    }
    CustomStringTableItem "{D09E099C90D56448}" {
     Id "LF-TestOrgSign_AlreadyIn"
     Target_en_us "You already belong to an organization. Leave it first."
     Comment "Greyed-out reason and toast: Join while already in an organisation (#934)."
    }
    CustomStringTableItem "{20EB5D2AA9C99C49}" {
     Id "LF-TestOrgSign_Denied"
     Target_en_us "Playtest only. This sign works in the playtest {town}."
     Comment "Toast when the playtest gate or the dev gate refuses the org sign (#934)."
    }
    CustomStringTableItem "{B249F0D4F960A3F9}" {
     Id "LF-TestOrgSign_Join"
     Target_en_us "Join test organization"
     Comment "F-key action on the playtest sign: join the shared Test Organisation (#934)."
    }
    CustomStringTableItem "{8E9D6E84A80978B5}" {
     Id "LF-TestOrgSign_Joined"
     Target_en_us "You are in the Test organization. The hideout door in the alley opens for you."
     Comment "Toast after joining the Test Organisation (#934)."
    }
    CustomStringTableItem "{920FCADF747D40B0}" {
     Id "LF-TestOrgSign_Leave"
     Target_en_us "Leave organization"
     Comment "F-key action on the playtest sign: leave your organisation, founder included (#934)."
    }
    CustomStringTableItem "{88BA7C53158F4683}" {
     Id "LF-TestOrgSign_Left"
     Target_en_us "You left the organization. Its door is shut to you now."
     Comment "Toast after leaving the organisation (#934)."
    }
    CustomStringTableItem "{50FAC5EDA66028A3}" {
     Id "LF-TestOrgSign_NoCharacter"
     Target_en_us "You have no character yet."
     Comment "Toast when the org sign is used with no character (#934)."
    }
    CustomStringTableItem "{4EA24B0F715B7041}" {
     Id "LF-TestOrgSign_NotIn"
     Target_en_us "You are not in an organization."
     Comment "Greyed-out reason and toast: Leave with no organisation (#934)."
    }
    CustomStringTableItem "{7581DEC7A2972FB8}" {
     Id "LF-TestOrgSign_OtherOrg"
     Target_en_us "The test sign only takes you out of the test organization."
     Comment "Toast: the test org sign's Leave refused because the player belongs to a different organisation (#2536)."
    }
    CustomStringTableItem "{C3D98F355E87E01F}" {
     Id "LF-TestOrgSign_Title"
     Target_en_us "Test organization"
     Comment "Toast title of the playtest organisation sign (#934)."
    }
    CustomStringTableItem "{213816480AB942FE}" {
     Id "LF-TestOrgSign_TooFar"
     Target_en_us "Step up to the sign."
     Comment "Toast when the org sign is used from too far (#934)."
    }
    CustomStringTableItem "{92F24781D1DE033B}" {
     Id "LF-TestOrgSign_YardTaken"
     Target_en_us "You are in the Test organization, but another organization holds the hideout yard."
     Comment "Toast after joining when the hideout lease belongs to someone else (#934)."
    }
    CustomStringTableItem "{A904CA1B8DB84484}" {
     Id "LF-Ticket_Amount"
     Target_en_us "Fine: %1"
     Comment "Ticket line. %1 is the amount owed now."
    }
    CustomStringTableItem "{60A12CD943F5CFC3}" {
     Id "LF-Ticket_Blank"
     Target_en_us "A blank police ticket. Nothing is written on it."
     Comment "Ticket item text before it is printed."
    }
    CustomStringTableItem "{18CE97A1737CC737}" {
     Id "LF-Ticket_CardClose"
     Target_en_us "Fold it away"
     Comment "Close button of the ticket card."
    }
    CustomStringTableItem "{81F80B9BFCEBBE61}" {
     Id "LF-Ticket_CardTitle"
     Target_en_us "{POLICE} TICKET"
     Comment "Title printed on the ticket card."
    }
    CustomStringTableItem "{F8790EF1AB943842}" {
     Id "LF-Ticket_DueLate"
     Target_en_us "OVERDUE: the {fine} went up. Pay within %1 min of your time in {town} or the {police} will want you."
     Comment "Ticket line after the first deadline. %1 is online minutes left before the holder becomes wanted."
    }
    CustomStringTableItem "{E43A03504CE37DC5}" {
     Id "LF-Ticket_DueOpen"
     Target_en_us "Pay within %1 min of your time in {town}. After that the {fine} rises to %2."
     Comment "Ticket line before the first deadline. %1 is online minutes left, %2 the raised amount. Time only counts while the holder is online."
    }
    CustomStringTableItem "{8758643952766548}" {
     Id "LF-Ticket_DueWanted"
     Target_en_us "UNPAID: the {police} want you for this {fine}. It can still be paid."
     Comment "Ticket line after the second deadline."
    }
    CustomStringTableItem "{2AED2A45EFDE680C}" {
     Id "LF-Ticket_HowToPay"
     Target_en_us "Pay in cash at the punch clock on the {police} station desk: Pay ticket. Not from the bank."
     Comment "Ticket line: where and how to pay. Pay ticket is the exact action name."
    }
    CustomStringTableItem "{A3922704BA30BE5B}" {
     Id "LF-Ticket_IssuedTo"
     Target_en_us "Issued to: %1"
     Comment "Ticket line. %1 is the holder's name."
    }
    CustomStringTableItem "{D3D038CBB83F2177}" {
     Id "LF-Ticket_Name"
     Target_en_us "Police ticket"
     Comment "Inventory name of the ticket item."
    }
    CustomStringTableItem "{10B81E05A9DBDA87}" {
     Id "LF-Ticket_NeedCash"
     Target_en_us "You need %1 in cash"
     Comment "Why Pay ticket cannot be done. %1 is the amount owed."
    }
    CustomStringTableItem "{BC9C33F03DC472F7}" {
     Id "LF-Ticket_NoneOpen"
     Target_en_us "You have no unpaid ticket."
     Comment "Toast when paying with no unpaid ticket."
    }
    CustomStringTableItem "{5C73DBF2463CCCB8}" {
     Id "LF-Ticket_NotEnoughCash"
     Target_en_us "You need %1 in cash to pay this ticket."
     Comment "Toast when the payer carries too little cash. %1 is the amount."
    }
    CustomStringTableItem "{DBEBFF247E50B272}" {
     Id "LF-Ticket_NowWanted"
     Target_en_us "You did not pay your ticket. The {police} want you."
     Comment "Toast at the second deadline."
    }
    CustomStringTableItem "{7033F4EADCC25C6B}" {
     Id "LF-Ticket_Number"
     Target_en_us "No. %1, written %2"
     Comment "Ticket line. %1 is the ticket number, %2 the in-game date and time it was written."
    }
    CustomStringTableItem "{3113A99FF057AF65}" {
     Id "LF-Ticket_Offence"
     Target_en_us "Offense: %1"
     Comment "Ticket line. %1 is the offence name."
    }
    CustomStringTableItem "{C5F76D31210D2903}" {
     Id "LF-Ticket_OffenceBrandishing"
     Target_en_us "Waving a weapon in public"
     Comment "Ticket offence."
    }
    CustomStringTableItem "{BB95BEB713FF0585}" {
     Id "LF-Ticket_OffenceCrash"
     Target_en_us "Causing a crash"
     Comment "Ticket offence. Above the patrol fine cap in the flagship config."
    }
    CustomStringTableItem "{F12F553D6113B682}" {
     Id "LF-Ticket_OffenceDisturbance"
     Target_en_us "Disturbing the peace"
     Comment "Ticket offence."
    }
    CustomStringTableItem "{0ED00662F4C50149}" {
     Id "LF-Ticket_OffenceRecklessDriving"
     Target_en_us "Reckless driving"
     Comment "Ticket offence."
    }
    CustomStringTableItem "{7323EEE3F63F8704}" {
     Id "LF-Ticket_OffenceSpeeding"
     Target_en_us "Speeding"
     Comment "Ticket offence."
    }
    CustomStringTableItem "{329E1C0F66D568D4}" {
     Id "LF-Ticket_Officer"
     Target_en_us "Officer: %1 %2"
     Comment "Ticket line. %1 is the officer's rank, %2 the officer's name."
    }
    CustomStringTableItem "{D88B2F96BB0FB203}" {
     Id "LF-Ticket_Overdue"
     Target_en_us "Your ticket is overdue. The {fine} is now %1."
     Comment "Toast at the first deadline. %1 is the raised amount."
    }
    CustomStringTableItem "{2AF7CEA623158FD6}" {
     Id "LF-Ticket_PadAboveRank"
     Target_en_us "%1, %2: senior officers only"
     Comment "Ticket pad line for an offence above the officer's rank cap. %1 is the offence, %2 its fixed fine."
    }
    CustomStringTableItem "{99C269ABF65CD6EC}" {
     Id "LF-Ticket_PadClose"
     Target_en_us "Put the pad away"
     Comment "Close button of the ticket pad."
    }
    CustomStringTableItem "{406456BDD3AE8B75}" {
     Id "LF-Ticket_PadNote"
     Target_en_us "They pay in cash at the {police} desk within %1 min of their time in {town}. Unpaid, it rises once, then they are wanted."
     Comment "Ticket pad footer. %1 is the first deadline in online minutes."
    }
    CustomStringTableItem "{8E6D57F980FC5B64}" {
     Id "LF-Ticket_PadPick"
     Target_en_us "Check one offense. Each has a fixed {fine}."
     Comment "Ticket pad instruction."
    }
    CustomStringTableItem "{DC2428C790D45A2D}" {
     Id "LF-Ticket_PadTitle"
     Target_en_us "{POLICE} TICKET"
     Comment "Title of the officer's ticket pad."
    }
    CustomStringTableItem "{BBEF205C1F2CB38F}" {
     Id "LF-Ticket_PadTo"
     Target_en_us "To: the person in front of you"
     Comment "Ticket pad line: who gets the ticket."
    }
    CustomStringTableItem "{461AFE9329D0D247}" {
     Id "LF-Ticket_PadWrite"
     Target_en_us "Write: %1, %2"
     Comment "Ticket pad button for one offence. %1 is the offence, %2 its fixed fine."
    }
    CustomStringTableItem "{46A53FAB3C5D54F3}" {
     Id "LF-Ticket_PaidLine"
     Target_en_us "PAID %1. Stamped at the {police} desk. Keep this as your receipt."
     Comment "Ticket line once paid. %1 is the in-game date and time of payment."
    }
    CustomStringTableItem "{A85CDAFEB439AA39}" {
     Id "LF-Ticket_PaidToast"
     Target_en_us "Ticket paid: %1. Keep the stamped receipt."
     Comment "Toast after paying at the desk. %1 is the amount."
    }
    CustomStringTableItem "{035F5F834128D1CA}" {
     Id "LF-Ticket_Pay"
     Target_en_us "Pay ticket No. %1 (%2)"
     Comment "Desk payment action. %1 is the ticket number, %2 the amount owed now."
    }
    CustomStringTableItem "{E42E60D056AAD09F}" {
     Id "LF-Ticket_PayDesc"
     Target_en_us "Pay a police ticket in carried cash. The clock stamps it as your receipt."
     Comment "Description of the desk payment action."
    }
    CustomStringTableItem "{97E5443DC92B1C10}" {
     Id "LF-Ticket_PayGeneric"
     Target_en_us "Pay ticket"
     Comment "Fallback name of the desk payment action."
    }
    CustomStringTableItem "{9A1C0620A84F75B1}" {
     Id "LF-Ticket_Read"
     Target_en_us "Read ticket"
     Comment "Action on a printed ticket lying in the world."
    }
    CustomStringTableItem "{C80206F362C570EC}" {
     Id "LF-Ticket_Received"
     Target_en_us "An officer gave you a ticket. Read it in your inventory; pay it at the {police} desk."
     Comment "Toast to the person who got a ticket."
    }
    CustomStringTableItem "{6BD98468BB91818B}" {
     Id "LF-Ticket_Refused"
     Target_en_us "You cannot write this ticket."
     Comment "Toast to the officer for any other refusal."
    }
    CustomStringTableItem "{0C17FD1924B2AF02}" {
     Id "LF-Ticket_Title"
     Target_en_us "{Police} ticket"
     Comment "Toast title for every ticket message."
    }
    CustomStringTableItem "{5EDD1F09C108B2CB}" {
     Id "LF-Ticket_TooMany"
     Target_en_us "This person already holds %1 unpaid tickets."
     Comment "Toast to the officer. %1 is the limit."
    }
    CustomStringTableItem "{4E677F2199216C44}" {
     Id "LF-Ticket_Write"
     Target_en_us "Write ticket"
     Comment "Player interaction entry for an on-duty officer facing a player: opens the ticket pad."
    }
    CustomStringTableItem "{F537077C4B4DF892}" {
     Id "LF-Ticket_Written"
     Target_en_us "Ticket handed over: %1"
     Comment "Toast to the officer. %1 is the amount."
    }
    CustomStringTableItem "{18DFC167284D07E8}" {
     Id "LF-ToolBench_Done"
     Target_en_us "Good as new. That came to %1 and one scrap metal."
     Comment "Bench repair done (#4187). %1 = fee"
    }
    CustomStringTableItem "{3E45956C16155502}" {
     Id "LF-ToolBench_Failed"
     Target_en_us "The bench cannot do repairs right now."
     Comment "Bench refusal on a broken setup (#4187)"
    }
    CustomStringTableItem "{8F6C7F1D0C427B09}" {
     Id "LF-ToolBench_Full"
     Target_en_us "That tool is already in top shape."
     Comment "Bench refusal (#4187)"
    }
    CustomStringTableItem "{25BBCAE014F0D78D}" {
     Id "LF-ToolBench_NoMoney"
     Target_en_us "You do not have enough cash for the repair."
     Comment "Bench refusal (#4187)"
    }
    CustomStringTableItem "{3088D8D49AFC71C9}" {
     Id "LF-ToolBench_NoScrap"
     Target_en_us "You need one piece of scrap metal for the repair."
     Comment "Bench refusal (#4187)"
    }
    CustomStringTableItem "{C6145C357DAD1FAB}" {
     Id "LF-ToolBench_NoTool"
     Target_en_us "Hold the tool you want worked on in your hands."
     Comment "Bench refusal (#4187)"
    }
    CustomStringTableItem "{6F3046628BDFDFCF}" {
     Id "LF-ToolBench_RepairPickaxe"
     Target_en_us "Repair the pickaxe"
     Comment "Hardware bench action (#4187)"
    }
    CustomStringTableItem "{C16B93140B289B82}" {
     Id "LF-ToolBench_RestringRod"
     Target_en_us "Re-string the rod"
     Comment "Hardware bench action (#4187)"
    }
    CustomStringTableItem "{1188B7A6ADC3C8C7}" {
     Id "LF-ToolBench_SharpenAxe"
     Target_en_us "Sharpen the axe"
     Comment "Hardware bench action (#4187)"
    }
    CustomStringTableItem "{62BCE351AF0CBFB2}" {
     Id "LF-ToolBench_Title"
     Target_en_us "Hardware bench"
     Comment "Notification title (#4187)"
    }
    CustomStringTableItem "{3723AB61E29CDCE1}" {
     Id "LF-ToolCondition_Good"
     Target_en_us "%1 is in good shape"
     Comment "Action text: tool condition at or above the good threshold (#4182). %1 = tool name"
    }
    CustomStringTableItem "{2E6DE213CABD02C9}" {
     Id "LF-ToolCondition_Poor"
     Target_en_us "%1 is badly worn and slow"
     Comment "Action text: tool condition in the poor band, working slower (#4182). %1 = tool name"
    }
    CustomStringTableItem "{C23C3D6D9E4BDB06}" {
     Id "LF-ToolCondition_Worn"
     Target_en_us "%1 is getting worn"
     Comment "Action text: tool condition in the worn band (#4182). %1 = tool name"
    }
    CustomStringTableItem "{DE472F7EE6D1E8FD}" {
     Id "LF-ToolCondition_WornOut"
     Target_en_us "%1 is worn out but still works, slowly"
     Comment "Action text: tool at zero condition; it still works at half speed (#4182). %1 = tool name"
    }
    CustomStringTableItem "{0B593FFD66EB831C}" {
     Id "LF-ToolKind_Axe"
     Target_en_us "Your axe"
     Comment "Tool kind name that fills %1 of the tool condition sentence (#4300)"
    }
    CustomStringTableItem "{DFCB9DADDFCEE73E}" {
     Id "LF-ToolKind_Pickaxe"
     Target_en_us "Your pickaxe"
     Comment "Tool kind name that fills %1 of the tool condition sentence (#4300)"
    }
    CustomStringTableItem "{80FF3472F8A2800D}" {
     Id "LF-ToolKind_Rod"
     Target_en_us "Your fishing rod"
     Comment "Tool kind name that fills %1 of the tool condition sentence (#4300)"
    }
    CustomStringTableItem "{75960D0103B97E17}" {
     Id "LF-ToolTier_Fine"
     Target_en_us "Fine"
     Comment "Tool tier name (#4182)"
    }
    CustomStringTableItem "{72AC08B33DD29667}" {
     Id "LF-ToolTier_Standard"
     Target_en_us "Standard"
     Comment "Tool tier name (#4182)"
    }
    CustomStringTableItem "{10F79179EEA9F6E8}" {
     Id "LF-ToolTier_Worn"
     Target_en_us "Worn"
     Comment "Tool tier name (#4182)"
    }
    CustomStringTableItem "{7038FBC3DB7D537B}" {
     Id "LF-TouristMap_Marked"
     Target_en_us "You pencil %1 new places onto your map."
     Comment "Tourist map item (#2242): toast after studying it. %1 = places new to the player's map."
    }
    CustomStringTableItem "{59CD849C9814E8C3}" {
     Id "LF-TouristMap_Nothing"
     Target_en_us "Your map already shows everything this one lists."
     Comment "Tourist map item (#2242): toast when it adds nothing."
    }
    CustomStringTableItem "{23CAFD3EFF424E4F}" {
     Id "LF-TouristMap_Title"
     Target_en_us "Tourist map"
     Comment "Tourist map item (#2242): the toast title."
    }
    CustomStringTableItem "{E999310EC10150C2}" {
     Id "LF-TouristMap_Use"
     Target_en_us "Study the tourist map"
     Comment "Tourist map item (#2242): the action that pencils every discoverable place onto the player's map."
    }
    CustomStringTableItem "{788218FBE4001F0E}" {
     Id "LF-Tow_Towed"
     Target_en_us "Your vehicle %1 was towed. Collect it at the impound desk."
     Comment "Toast to the owner: their abandoned car was towed (#2657). %1 is the plate"
    }
    CustomStringTableItem "{65D7E4795D634FBD}" {
     Id "LF-TownBoard_Dir_E"
     Target_en_us "east"
     Comment "Compass direction in a town board where line (LF-TownBoard_Where %3)."
    }
    CustomStringTableItem "{4B11C0ECDDCE8189}" {
     Id "LF-TownBoard_Dir_N"
     Target_en_us "north"
     Comment "Compass direction in a town board where line (LF-TownBoard_Where %3)."
    }
    CustomStringTableItem "{80F58ACCD4A1C233}" {
     Id "LF-TownBoard_Dir_NE"
     Target_en_us "north-east"
     Comment "Compass direction in a town board where line (LF-TownBoard_Where %3)."
    }
    CustomStringTableItem "{A38A10DDFC45AF7C}" {
     Id "LF-TownBoard_Dir_NW"
     Target_en_us "north-west"
     Comment "Compass direction in a town board where line (LF-TownBoard_Where %3)."
    }
    CustomStringTableItem "{5168FCB118D8BC18}" {
     Id "LF-TownBoard_Dir_S"
     Target_en_us "south"
     Comment "Compass direction in a town board where line (LF-TownBoard_Where %3)."
    }
    CustomStringTableItem "{7CB6ADB42B55E8EF}" {
     Id "LF-TownBoard_Dir_SE"
     Target_en_us "south-east"
     Comment "Compass direction in a town board where line (LF-TownBoard_Where %3)."
    }
    CustomStringTableItem "{022D42E11871EFF4}" {
     Id "LF-TownBoard_Dir_SW"
     Target_en_us "south-west"
     Comment "Compass direction in a town board where line (LF-TownBoard_Where %3)."
    }
    CustomStringTableItem "{C708215620CD3A47}" {
     Id "LF-TownBoard_Dir_W"
     Target_en_us "west"
     Comment "Compass direction in a town board where line (LF-TownBoard_Where %3)."
    }
    CustomStringTableItem "{00286F357D4A57D8}" {
     Id "LF-TownBoard_Mark"
     Target_en_us "Set waypoint"
     Comment "Button on a town board card's sheet: puts a marker over the place on the screen. No travel."
    }
    CustomStringTableItem "{5CF22CABB2E8D8C8}" {
     Id "LF-TownBoard_NoPlace"
     Target_en_us "Ask around"
     Comment "Where line of a town board card whose place is not on this map."
    }
    CustomStringTableItem "{99C4660CE7F6A41D}" {
     Id "LF-TownBoard_Title"
     Target_en_us "Notice board"
     Comment "Header printed across the top of the town board (the cork board of things to do). A pack overrides it with the town's name."
    }
    CustomStringTableItem "{5B0C1BA1D4744F19}" {
     Id "LF-TownBoard_Unmark"
     Target_en_us "Clear waypoint"
     Comment "Button on a town board card's sheet when that card's place is already the waypoint: takes the marker away."
    }
    CustomStringTableItem "{FB7DFFA399262048}" {
     Id "LF-TownBoard_Where"
     Target_en_us "%1, %2 m %3"
     Comment "Where a town board card's place is. %1 place name, %2 distance in metres, %3 a compass direction (LF-TownBoard_Dir_*). Example: Post office, 60 m east."
    }
    CustomStringTableItem "{2D9C41CAF21653BE}" {
     Id "LF-TownBoard_WhereLabel"
     Target_en_us "Where"
     Comment "Section label on a town board card's sheet, above where the place is."
    }
    CustomStringTableItem "{30CA1A24F0A2C32A}" {
     Id "LF-TownBoard_WhereNear"
     Target_en_us "%1, right here"
     Comment "Where a town board card's place is when it stands beside the board. %1 place name."
    }
    CustomStringTableItem "{527C8125A5DCF53B}" {
     Id "LF-Trader_NotBuyingNow"
     Target_en_us "Not buying a single one right now"
     Comment "Why a Sell 1 row is greyed: the buyer is full, or sells only by the lot (Sell all still works)."
    }
    CustomStringTableItem "{308E50CB824EB64C}" {
     Id "LF-Trader_NothingToSell"
     Target_en_us "You carry nothing this buyer takes"
     Comment "Why the sell-all row is greyed: no carried good is on this buyer's list."
    }
    CustomStringTableItem "{3C7E19B2D54A806F}" {
     Id "LF-Trader_SaleRefused"
     Target_en_us "The trader won't take that."
     Comment "Toast to the seller: the server refused a trader sale (not bought, out of reach, not carried, or the black market)."
    }
    CustomStringTableItem "{8D20F4A6B1957C3E}" {
     Id "LF-Trader_SaleRefused_Title"
     Target_en_us "No sale"
     Comment "Title of the refused-sale toast."
    }
    CustomStringTableItem "{683D10EE9EDCCDF2}" {
     Id "LF-Trader_SellAll"
     Target_en_us "Sell all goods (%1): {currency:sym}%2"
     Comment "Trader row that sells everything the buyer takes. %1 = units, %2 = the total price."
    }
    CustomStringTableItem "{73CDC5E30D79E666}" {
     Id "LF-Trader_SellAllAsk"
     Target_en_us "Sell all goods (%1)"
     Comment "Sell-all row before the prices arrived. %1 = units."
    }
    CustomStringTableItem "{544BAF6FFDCEE018}" {
     Id "LF-Trader_SellNothing"
     Target_en_us "Sell goods"
     Comment "Sell-all row when the player carries nothing this buyer takes (greyed)."
    }
    CustomStringTableItem "{114E334DB022DD7C}" {
     Id "LF-Trader_SellOne"
     Target_en_us "Sell 1 %1: {currency:sym}%2"
     Comment "Trader sell row for one item. %1 = the good, %2 = the price the buyer pays now."
    }
    CustomStringTableItem "{C5D743750A8BBD92}" {
     Id "LF-Trader_SellOneAsk"
     Target_en_us "Sell 1 %1"
     Comment "Trader sell row for one item before the price arrived. %1 = the good."
    }
    CustomStringTableItem "{0C4F96543148B6FF}" {
     Id "LF-Trader_SellRowName"
     Target_en_us "Sell"
     Comment "Fallback name of a trader sell row; the row shows its own good and price."
    }
    CustomStringTableItem "{B976D15E0E9B0B4B}" {
     Id "LF-Training_Course_detective_course"
     Target_en_us "Detective"
     Comment "Training desk: course display name."
    }
    CustomStringTableItem "{78F252053EA90564}" {
     Id "LF-Training_Course_first_aid_course"
     Target_en_us "First Aid"
     Comment "Training desk: course display name."
    }
    CustomStringTableItem "{B100F982AE5EF187}" {
     Id "LF-Training_Course_paramedic_course"
     Target_en_us "Paramedic"
     Comment "Training desk: course display name."
    }
    CustomStringTableItem "{2616F2C90F266CD7}" {
     Id "LF-Training_Course_traffic_course"
     Target_en_us "Traffic"
     Comment "Training desk: course display name."
    }
    CustomStringTableItem "{86202AB25CDA24C1}" {
     Id "LF-Training_Desc"
     Target_en_us "Hold to study. The fee is paid when you finish."
     Comment "Training desk: action description."
    }
    CustomStringTableItem "{90AE055011CAE1B5}" {
     Id "LF-Training_NotOnDuty"
     Target_en_us "Clock in at the duty desk before you take a course."
     Comment "Training desk: refused while off duty."
    }
    CustomStringTableItem "{7C378EF666F77691}" {
     Id "LF-Training_Qualified"
     Target_en_us "You've qualified: %1"
     Comment "Training desk: %1 is the course name, e.g. Detective."
    }
    CustomStringTableItem "{45E04447DEFE233C}" {
     Id "LF-Training_Take_detective_course"
     Target_en_us "Take the Detective course"
     Comment "Training desk action."
    }
    CustomStringTableItem "{F77B74BC9E316996}" {
     Id "LF-Training_Take_first_aid_course"
     Target_en_us "Take the First Aid course"
     Comment "Training desk action."
    }
    CustomStringTableItem "{0AED5736C8FBD7FB}" {
     Id "LF-Training_Take_paramedic_course"
     Target_en_us "Take the Paramedic course"
     Comment "Training desk action."
    }
    CustomStringTableItem "{735DD85BF3F2A589}" {
     Id "LF-Training_Take_traffic_course"
     Target_en_us "Take the Traffic course"
     Comment "Training desk action."
    }
    CustomStringTableItem "{F3F6CF6F64F0E4DA}" {
     Id "LF-Training_Title"
     Target_en_us "Training desk"
     Comment "Training desk: toast title."
    }
    CustomStringTableItem "{60A9BDB3CFCA73FA}" {
     Id "LF-TruckCheck_Action"
     Target_en_us "Take the truck check ({currency:sym}300)"
     Comment "Action on the examiner: pay the fee and take the lent truck for the licence check (#4696)."
    }
    CustomStringTableItem "{F0C30EB379F299C1}" {
     Id "LF-TruckCheck_ActionPlain"
     Target_en_us "Take the truck check"
     Comment "Prefab name of the examiner action; the script shows LF-TruckCheck_Action with the fee (#4696)."
    }
    CustomStringTableItem "{AFE6468581F2B50C}" {
     Id "LF-TruckCheck_FailedAway"
     Target_en_us "You walked away from the truck. Check failed. Retake in about %1 minutes."
     Comment "Toast: 300 m from the lent truck. %1 = retake wait in in-game minutes."
    }
    CustomStringTableItem "{B23BECAA67E06B1C}" {
     Id "LF-TruckCheck_FailedCourse"
     Target_en_us "Wrong gate. Check failed. Retake in about %1 minutes."
     Comment "Toast: the truck drove through a gate out of order. %1 = retake wait in in-game minutes."
    }
    CustomStringTableItem "{D6AC02B80EEAA64F}" {
     Id "LF-TruckCheck_FailedLeft"
     Target_en_us "You left the truck too long. Check failed. Retake in about %1 minutes."
     Comment "Toast: out of the lent truck for 60 s. %1 = retake wait in in-game minutes."
    }
    CustomStringTableItem "{87A36948E99E3AA1}" {
     Id "LF-TruckCheck_FailedWreck"
     Target_en_us "The truck is wrecked. Check failed. Retake in about %1 minutes."
     Comment "Toast: the lent truck was destroyed or is gone. %1 = retake wait in in-game minutes."
    }
    CustomStringTableItem "{44D0650A3100C463}" {
     Id "LF-TruckCheck_Gate"
     Target_en_us "Gate %1 of 4. Drive on to the next one."
     Comment "Toast: the lent truck cleared a gate in order. %1 = the gate number, 1 to 4."
    }
    CustomStringTableItem "{13750B36D26E7656}" {
     Id "LF-TruckCheck_Held"
     Target_en_us "You already hold the truck license."
     Comment "Examiner: the player has the truck license."
    }
    CustomStringTableItem "{822EE47BBD34375A}" {
     Id "LF-TruckCheck_NoCash"
     Target_en_us "The truck check costs {currency:sym}%1. Bring the cash."
     Comment "Examiner: the player carries less than the fee. %1 = the fee."
    }
    CustomStringTableItem "{EAF52394C7253DE0}" {
     Id "LF-TruckCheck_Passed"
     Target_en_us "You passed the truck check. The truck license is yours."
     Comment "Toast: the fourth gate was cleared; the truck license is granted."
    }
    CustomStringTableItem "{380BE3A3C6FE2658}" {
     Id "LF-TruckCheck_RetakeWait"
     Target_en_us "You failed the last check. Come back in about %1 minutes."
     Comment "Examiner: the retake wait has not ended. %1 = in-game minutes left."
    }
    CustomStringTableItem "{A91A792F66DB0BA1}" {
     Id "LF-TruckCheck_Running"
     Target_en_us "You are already on a truck check."
     Comment "Examiner: a check run is already open for the player."
    }
    CustomStringTableItem "{918ADAB133D71046}" {
     Id "LF-TruckCheck_Started"
     Target_en_us "The truck is yours for the check. Climb in and drive the gates in order."
     Comment "Examiner: the fee is paid, the truck is lent and the run is open."
    }
    CustomStringTableItem "{54F1741326492E7E}" {
     Id "LF-TruckCheck_Title"
     Target_en_us "Truck check"
     Comment "Toast title for every truck check answer (#4696)."
    }
    CustomStringTableItem "{1BD47FB3CC76FC81}" {
     Id "LF-TruckCheck_Unavailable"
     Target_en_us "The examiner cannot lend a truck right now."
     Comment "Examiner: the truck could not be lent (no truck prefab or the gates refused)."
    }
    CustomStringTableItem "{D820B2BCD7FC410A}" {
     Id "LF-UIKit_Keep"
     Target_en_us "Keep %1"
     Comment "UI kit quantity face for the amount that stays. %1 is the count."
    }
    CustomStringTableItem "{AE54ADB762D0B6FC}" {
     Id "LF-UIKit_SampleAction"
     Target_en_us "Take"
     Comment "UI kit sample status-row action. The server decides whether it stays."
    }
    CustomStringTableItem "{E9907671573AFD76}" {
     Id "LF-UIKit_SampleBalance"
     Target_en_us "Balance"
     Comment "UI kit sample line that a later caller rewrites with a server figure."
    }
    CustomStringTableItem "{54D943A7D936E1D0}" {
     Id "LF-UIKit_SampleButton"
     Target_en_us "File the form"
     Comment "UI kit sample button."
    }
    CustomStringTableItem "{950DC138F92AF1A2}" {
     Id "LF-UIKit_SampleClose"
     Target_en_us "Close"
     Comment "UI kit sample close button."
    }
    CustomStringTableItem "{F75004C4210A4AB1}" {
     Id "LF-UIKit_SampleField"
     Target_en_us "Name"
     Comment "UI kit sample edit box placeholder."
    }
    CustomStringTableItem "{68605FDBA4C10460}" {
     Id "LF-UIKit_SampleLine"
     Target_en_us "Write the name, then file the form at the counter."
     Comment "UI kit sample body line."
    }
    CustomStringTableItem "{55960B5ABA019A5E}" {
     Id "LF-UIKit_SampleQuantity"
     Target_en_us "Split the stack"
     Comment "UI kit sample quantity label."
    }
    CustomStringTableItem "{7AEB1C53F8FECFAD}" {
     Id "LF-UIKit_SampleRow"
     Target_en_us "Duty roster"
     Comment "UI kit sample list row."
    }
    CustomStringTableItem "{81CFDCDC3F645AEA}" {
     Id "LF-UIKit_SampleStatus"
     Target_en_us "Open"
     Comment "UI kit sample status-row status."
    }
    CustomStringTableItem "{3332B12FDA94C221}" {
     Id "LF-UIKit_SampleStatusLabel"
     Target_en_us "Warrant"
     Comment "UI kit sample status-row label."
    }
    CustomStringTableItem "{9E06D8383590F6C7}" {
     Id "LF-UIKit_SampleTitle"
     Target_en_us "Field office"
     Comment "UI kit sample screen title. A 1989 counter, not a desktop app."
    }
    CustomStringTableItem "{7C0A925F60789085}" {
     Id "LF-UIKit_Take"
     Target_en_us "Take %1"
     Comment "UI kit quantity face for the amount that leaves. %1 is the count."
    }
    CustomStringTableItem "{3333A1C0E5D70001}" {
     Id "LF-Unflip_Action"
     Target_en_us "Push it back over"
     Comment "Action name on an overturned empty vehicle (#3333)."
    }
    CustomStringTableItem "{3333A1C0E5D70003}" {
     Id "LF-Unflip_Cooldown"
     Target_en_us "It was only just pushed over. Give it a moment."
     Comment "Unflip refusal toast (#3333): inside the cooldown."
    }
    CustomStringTableItem "{3333A1C0E5D70005}" {
     Id "LF-Unflip_NotOverturned"
     Target_en_us "It is already on its wheels."
     Comment "Unflip refusal toast (#3333): the vehicle is not overturned."
    }
    CustomStringTableItem "{3333A1C0E5D70002}" {
     Id "LF-Unflip_Occupied"
     Target_en_us "Someone is still inside."
     Comment "Unflip refusal toast (#3333): the vehicle has an occupant."
    }
    CustomStringTableItem "{3333A1C0E5D70004}" {
     Id "LF-Unflip_TooFar"
     Target_en_us "Get closer to push it over."
     Comment "Unflip refusal toast (#3333): the player is out of reach."
    }
    CustomStringTableItem "{64FAA43ACB2D5BCD}" {
     Id "LF-UnitStatus_ActionAvailable"
     Target_en_us "Radio: available"
     Comment "Police car action row: tell dispatch this unit is free for calls."
    }
    CustomStringTableItem "{8F7C7A324959094B}" {
     Id "LF-UnitStatus_ActionBusy"
     Target_en_us "Radio: busy"
     Comment "Police car action row: tell dispatch this unit is tied up."
    }
    CustomStringTableItem "{6735AE870F08B8D7}" {
     Id "LF-UnitStatus_Available"
     Target_en_us "available"
     Comment "Unit status word on the dispatch board."
    }
    CustomStringTableItem "{E4EDC2B539CB3C85}" {
     Id "LF-UnitStatus_BoardHeading"
     Target_en_us "UNITS ON THE RADIO"
     Comment "Wanted board, police view: heading above the unit status rows."
    }
    CustomStringTableItem "{82942B511449462F}" {
     Id "LF-UnitStatus_BoardLine"
     Target_en_us "Unit %1: %2"
     Comment "Dispatch board unit row. %1 = officer name, %2 = available or busy."
    }
    CustomStringTableItem "{A403887D50552271}" {
     Id "LF-UnitStatus_Busy"
     Target_en_us "busy"
     Comment "Unit status word on the dispatch board."
    }
    CustomStringTableItem "{0877B5083A43C81C}" {
     Id "LF-UnitStatus_NoName"
     Target_en_us "unnamed officer"
     Comment "Dispatch board unit row when the officer has no name."
    }
    CustomStringTableItem "{C5AB110336544DB4}" {
     Id "LF-UnitStatus_SetAvailable"
     Target_en_us "Dispatch has you as available."
     Comment "Toast after the officer radios available."
    }
    CustomStringTableItem "{0A69E149174D2D14}" {
     Id "LF-UnitStatus_SetBusy"
     Target_en_us "Dispatch has you as busy."
     Comment "Toast after the officer radios busy."
    }
    CustomStringTableItem "{296F37B4C207EAD9}" {
     Id "LF-UnitStatus_Title"
     Target_en_us "Radio"
     Comment "Toast title for unit status radio messages."
    }
    CustomStringTableItem "{7B2D91E4A0C358F6}" {
     Id "LF-VehicleCompartment_Open"
     Target_en_us "Open the panel"
     Comment "Action on a fitted car, shown to its owner only: opens the hidden compartment (#3434)."
    }
    CustomStringTableItem "{2E84C0B7D1596A3F}" {
     Id "LF-VehicleCompartment_Title"
     Target_en_us "Hidden compartment"
     Comment "Title of a car's hidden compartment storage (#3434)."
    }
    CustomStringTableItem "{C470CD4C9E58C70D}" {
     Id "LF-VehicleLock_NoTarget"
     Target_en_us "No vehicle of yours in reach"
     Comment "Toast when U is pressed with no keyed vehicle near."
    }
    CustomStringTableItem "{4466EFBBF7C1275F}" {
     Id "LF-VehicleSearch_ClassAlcohol"
     Target_en_us "alcohol"
     Comment "Contraband class in a stop search (#4196): untaxed vodka and spirits."
    }
    CustomStringTableItem "{1C42F4FE39BCD2FB}" {
     Id "LF-VehicleSearch_ClassLine"
     Target_en_us "%1 %2"
     Comment "One class in a stop-search find (#4196). %1 = units, %2 = the class name."
    }
    CustomStringTableItem "{360DF5FF0A09FDAB}" {
     Id "LF-VehicleSearch_ClassNarcotics"
     Target_en_us "narcotics"
     Comment "Contraband class in a stop search (#4196): drugs."
    }
    CustomStringTableItem "{A7EBBB8DE2636103}" {
     Id "LF-VehicleSearch_ClassTobacco"
     Target_en_us "tobacco"
     Comment "Contraband class in a stop search (#4196): cigarette cartons."
    }
    CustomStringTableItem "{9F64D10C6C809AF4}" {
     Id "LF-VehicleSearch_ClassUntaxed"
     Target_en_us "untaxed goods"
     Comment "Contraband class in a stop search (#4196): other goods with no tax stamp."
    }
    CustomStringTableItem "{8000AB7071970A0B}" {
     Id "LF-VehicleSearch_Found"
     Target_en_us "Found in the trunk: %1. Search again to seize it."
     Comment "Toast to the officer: a stop search found contraband (#4196). %1 = units by class, e.g. '2 tobacco, 1 alcohol'."
    }
    CustomStringTableItem "{1757B26C4D55D5D3}" {
     Id "LF-VehicleSearch_Seized"
     Target_en_us "Seized %1 contraband item(s) from the car"
     Comment "Toast to the officer: the second search seized the goods it found (#4196). %1 = units seized."
    }
    CustomStringTableItem "{2606D4AE3E7A4FE0}" {
     Id "LF-Vehicle_BankShort"
     Target_en_us "Cars are paid from your bank account, not from cash. Deposit cash at an ATM, then buy."
     Comment "Vehicle purchase refused: the price is above the cash limit and the bank balance is short"
    }
    CustomStringTableItem "{3304D1C0A7E25B91}" {
     Id "LF-Vehicle_NeedsRallyDlc"
     Target_en_us "This tractor needs the Rally DLC."
     Comment "Vehicle purchase refused: the lot does not sell the Rally tractor to a player without the DLC (#3304)"
    }
    CustomStringTableItem "{A13F2CC58EFA7C05}" {
     Id "LF-Vehicle_OutOnLot"
     Target_en_us "Your car, plate %1, is out on the lot. Its hazard lights are blinking and a marker sits over it for 30 seconds."
     Comment "Toast when the garage hands a car out. %1 = the plate."
    }
    CustomStringTableItem "{D453CA5B325AB1E5}" {
     Id "LF-Vehicle_OutOnLotNoPlate"
     Target_en_us "Your car is out on the lot. Its hazard lights are blinking and a marker sits over it for 30 seconds."
     Comment "Toast when the garage hands out a car that has no plate."
    }
    CustomStringTableItem "{FB09B2B3594C991F}" {
     Id "LF-Vehicle_OutOnLotTitle"
     Target_en_us "Your car"
     Comment "Toast title when the garage hands a car out to its owner."
    }
    CustomStringTableItem "{6D1F8A3E2C94B057}" {
     Id "LF-Vehicle_Ural"
     Target_en_us "Ural 4320 cargo truck"
     Comment "Lot catalog row for the Ural cargo truck (#4417)."
    }
    CustomStringTableItem "{5527F4187C4874BD}" {
     Id "LF-Vehicle_YourCarMark"
     Target_en_us "Your car"
     Comment "Waypoint label over the car the garage just handed out."
    }
    CustomStringTableItem "{5E0B7D31C9A24F86}" {
     Id "LF-Wanted_BuildHidden"
     Target_en_us "build hidden"
     Comment "Witness report and wanted poster clothing list: something the suspect wore hid the build."
    }
    CustomStringTableItem "{DA0581F22B573242}" {
     Id "LF-Wanted_Driving"
     Target_en_us "driving %1"
     Comment "Wanted poster: %1 is the vehicle name."
    }
    CustomStringTableItem "{A1C7E94B2D603F58}" {
     Id "LF-Wanted_FaceHidden"
     Target_en_us "face hidden"
     Comment "Witness report and wanted poster clothing list: something the suspect wore covered the face."
    }
    CustomStringTableItem "{2AE926B35D391FDA}" {
     Id "LF-Wanted_Level"
     Target_en_us "wanted level %1 of 5"
     Comment "Wanted poster headline: how wanted the suspect is now. %1 is 1 to 5."
    }
    CustomStringTableItem "{D733E65634209297}" {
     Id "LF-Wanted_NoDescription"
     Target_en_us "no description"
     Comment "Wanted poster: nothing was seen of the clothing."
    }
    CustomStringTableItem "{208346B2A0835221}" {
     Id "LF-Wanted_None"
     Target_en_us "No one is wanted right now."
     Comment "Wanted board card when the board is empty."
    }
    CustomStringTableItem "{66B7B59C6D36A297}" {
     Id "LF-Wanted_OnFoot"
     Target_en_us "on foot"
     Comment "Wanted poster: the suspect was not in a vehicle."
    }
    CustomStringTableItem "{7D1E5B0A94C2F368}" {
     Id "LF-Wanted_PlaceAt"
     Target_en_us "%1, %2"
     Comment "A crime place inside a town (#5147): %1 is the site's own name (a sign title), %2 the town. e.g. Fuel Station, Saint Philippe."
    }
    CustomStringTableItem "{35F30778D3B26361}" {
     Id "LF-Wanted_PlaceGrid"
     Target_en_us "grid %1"
     Comment "Place on a wanted poster when no sign is near. %1 is the map grid square, e.g. 002-002."
    }
    CustomStringTableItem "{DB6449E40E448E46}" {
     Id "LF-Wanted_PosterDriving"
     Target_en_us "Driving %1"
     Comment "Wanted poster on the board's face, when nothing was seen of the clothing: %1 is the vehicle name."
    }
    CustomStringTableItem "{4AD8545DB4254A00}" {
     Id "LF-Wanted_PosterHeader"
     Target_en_us "Wanted"
     Comment "Headline printed at the top of a wanted poster on the board's face; shown in capitals."
    }
    CustomStringTableItem "{6299E3E0908A6C21}" {
     Id "LF-Wanted_PosterStolen"
     Target_en_us "Stolen"
     Comment "Headline of a stolen-car poster on the wanted board's face; shown in capitals."
    }
    CustomStringTableItem "{E060F7BC110E80EE}" {
     Id "LF-Wanted_PosterStolenTip"
     Target_en_us "Seen it? Tell the {police}."
     Comment "Stolen-car poster on the wanted board's face: the appeal under the plate and the car's name."
    }
    CustomStringTableItem "{E067EA2BE746BA32}" {
     Id "LF-Wanted_PublicTip"
     Target_en_us "Seen them? Tell an officer."
     Comment "Line on the public wanted poster card."
    }
    CustomStringTableItem "{0C509872EDE3C2C8}" {
     Id "LF-Wanted_Read"
     Target_en_us "Read the wanted posters"
     Comment "Action on the station wanted board."
    }
    CustomStringTableItem "{92CC57DB2785885E}" {
     Id "LF-Wanted_SeenAt"
     Target_en_us "last seen at %1"
     Comment "Wanted poster, police only: %1 is the place of the crime."
    }
    CustomStringTableItem "{BD4E87FFF2BC3717}" {
     Id "LF-Wanted_Title"
     Target_en_us "WANTED"
     Comment "Title of the public wanted poster card."
    }
    CustomStringTableItem "{9E5C42E17A717873}" {
     Id "LF-Wanted_TitlePolice"
     Target_en_us "WANTED - {police} notices"
     Comment "Title of the wanted board card an on-duty officer reads."
    }
    CustomStringTableItem "{9A51FA9C497B307F}" {
     Id "LF-Wanted_Unknown"
     Target_en_us "Unknown person"
     Comment "Wanted poster headline before the suspect is identified."
    }
    CustomStringTableItem "{8C5D85EC61903BF8}" {
     Id "LF-Wanted_VehicleUnknown"
     Target_en_us "a vehicle"
     Comment "Wanted poster: the suspect drove a vehicle with no display name."
    }
    CustomStringTableItem "{9DFCE698AEB990D9}" {
     Id "LF-Wanted_Wearing"
     Target_en_us "Wearing %1"
     Comment "Wanted poster description. %1 is a list of clothing names."
    }
    CustomStringTableItem "{79517D819FE7FE2B}" {
     Id "LF-Waypoint_Distance"
     Target_en_us "%1 m"
     Comment "Distance under the waypoint marker on the screen. %1 metres."
    }
    CustomStringTableItem "{7AF5414FC3686A70}" {
     Id "LF-Weapon_Taser_Description"
     Target_en_us "Police stun pistol. One cartridge. A hit within ten meters puts a person down for a few seconds and does not wound."
     Comment "Inventory description of the police stun pistol."
    }
    CustomStringTableItem "{963D39B8BF62B9C8}" {
     Id "LF-Weapon_Taser_Name"
     Target_en_us "Taser"
     Comment "Inventory name of the police stun pistol."
    }
    CustomStringTableItem "{092A6842A1A3E19E}" {
     Id "LF-WelcomeGuide_Body1a"
     Target_en_us "You are new in {town} and carry no papers. Your first stop is the registry clerk: he files your papers, hands you your ID card and gives you your first errands."
     Comment "Welcome guide page 1, first paragraph: get the ID card at the clerk. Core text, no place names (the pack overrides it)."
    }
    CustomStringTableItem "{0C5F82D29C7B928C}" {
     Id "LF-WelcomeGuide_Body1b"
     Target_en_us "The quest panel says what to do next, and a waypoint with the distance shows the way to each errand. For your first minutes nobody can rob you."
     Comment "Welcome guide page 1, second paragraph: the arrival quest marks and the grace period."
    }
    CustomStringTableItem "{74258C62AD985DC2}" {
     Id "LF-WelcomeGuide_Body2a"
     Target_en_us "Cash in your pocket is lost when you die; money in the bank is not. Put cash in or take it out at any ATM or at the bank teller."
     Comment "Welcome guide page 2, first paragraph: cash and the bank."
    }
    CustomStringTableItem "{6C722FC34D331592}" {
     Id "LF-WelcomeGuide_Body2b"
     Target_en_us "Starter jobs pay at once. A courier run from the post office pays for every parcel you deliver, and the notice board lists every place in {town} that hires, each with a waypoint button."
     Comment "Welcome guide page 2, second paragraph: starter jobs. Core text, no place names (the pack overrides it)."
    }
    CustomStringTableItem "{96A07E407C8D2B73}" {
     Id "LF-WelcomeGuide_Body3a"
     Target_en_us "Press M to open the map. If nothing opens, take the folded paper map out of your pack and hold it."
     Comment "Welcome guide page 3, first paragraph: opening the map."
    }
    CustomStringTableItem "{B73B6D99D63DC09B}" {
     Id "LF-WelcomeGuide_Body3b"
     Target_en_us "Shift + Left click on the map (gamepad: hold Y, press A) sets your own waypoint; it stands in the world with the distance to it. There is one at a time. Backspace (gamepad: double-tap Y) clears it."
     Comment "Welcome guide page 3, second paragraph: setting a waypoint on the map."
    }
    CustomStringTableItem "{F5C02DE4953A73B3}" {
     Id "LF-WelcomeGuide_Body4a"
     Target_en_us "{Police} officers and {medic:plural} are players. Anyone can sign on: press F at the duty desk in the {police} station or the clinic and clock in. Pay is handed over in cash when you clock out."
     Comment "Welcome guide page 4, first paragraph: going on duty. Core text, no place names (the pack overrides it)."
    }
    CustomStringTableItem "{1C067E60B6FF0E64}" {
     Id "LF-WelcomeGuide_Body4b"
     Target_en_us "If you are shot you go down, not dead. A {medic} on duty can revive you; otherwise you wake in the hospital with a bill to pay at its till."
     Comment "Welcome guide page 4, second paragraph: downed, revive, hospital bill."
    }
    CustomStringTableItem "{DE7156D698BB43DD}" {
     Id "LF-WelcomeGuide_Body5a"
     Target_en_us "The player manual explains jobs, homes, shops, the {police} and the {medic:plural} in full: life-framework.org/manual/. The server rules are in the Handbook in the pause menu (Escape)."
     Comment "Welcome guide page 5, first paragraph: the manual link and the handbook."
    }
    CustomStringTableItem "{A2958ED54A2031BF}" {
     Id "LF-WelcomeGuide_Body5b"
     Target_en_us "Press F3 (gamepad: right bumper and Y) to open this guide again at any time. You can change the key in Settings, Controls."
     Comment "Welcome guide page 5, second paragraph: the reopen key (LF_WelcomeGuide, chimeraInputCommon.conf)."
    }
    CustomStringTableItem "{B6132A753F1BF8B9}" {
     Id "LF-WelcomeGuide_Close"
     Target_en_us "Close"
     Comment "Welcome guide: close button; the guide opens by itself again at the next join until Don't show again."
    }
    CustomStringTableItem "{AABF7919DBA9BDBB}" {
     Id "LF-WelcomeGuide_Dismiss"
     Target_en_us "Don't show again"
     Comment "Welcome guide: button that stops the guide opening by itself at join; the key still opens it."
    }
    CustomStringTableItem "{31AE6F2A1A2047DC}" {
     Id "LF-WelcomeGuide_Dismissed"
     Target_en_us "The guide will not open by itself again. Press F3 to read it."
     Comment "Toast after Don't show again."
    }
    CustomStringTableItem "{3D6B90E2A41C85F7}" {
     Id "LF-WelcomeGuide_DoneBody1a"
     Target_en_us "Your arrival errands are done and your papers are in order. Open your personal file (F2) and pick What to do next: it lists ways to earn a living and first steps around {town}. Press Start on one and it puts a waypoint on the first step; Resume puts the waypoint back on one you already started."
     Comment "Welcome guide page 1, first paragraph once every arrival errand is turned in. Core text, no place names."
    }
    CustomStringTableItem "{A92F17D3586E04BC}" {
     Id "LF-WelcomeGuide_DoneBody1b"
     Target_en_us "Press M for the map. The next pages cover money, the map, the {police} and the {medic:plural}, and where to read more."
     Comment "Welcome guide page 1, second paragraph once every arrival errand is turned in."
    }
    CustomStringTableItem "{F4C80B6195DA3E27}" {
     Id "LF-WelcomeGuide_DoneHead1"
     Target_en_us "WHAT NEXT"
     Comment "Welcome guide page 1 heading once every arrival errand is turned in."
    }
    CustomStringTableItem "{08C7D7374201626B}" {
     Id "LF-WelcomeGuide_Head1"
     Target_en_us "FIRST STEPS"
     Comment "Welcome guide page 1 heading."
    }
    CustomStringTableItem "{67E6BE787D1A987F}" {
     Id "LF-WelcomeGuide_Head2"
     Target_en_us "MONEY"
     Comment "Welcome guide page 2 heading."
    }
    CustomStringTableItem "{8CBEE5B95BE40EC0}" {
     Id "LF-WelcomeGuide_Head3"
     Target_en_us "THE MAP"
     Comment "Welcome guide page 3 heading."
    }
    CustomStringTableItem "{17589246A8C8CBDB}" {
     Id "LF-WelcomeGuide_Head4"
     Target_en_us "ON DUTY"
     Comment "Welcome guide page 4 heading."
    }
    CustomStringTableItem "{B533A055F306522E}" {
     Id "LF-WelcomeGuide_Head5"
     Target_en_us "MORE HELP"
     Comment "Welcome guide page 5 heading."
    }
    CustomStringTableItem "{B591033A5B47E1C2}" {
     Id "LF-WelcomeGuide_Key"
     Target_en_us "Open the welcome guide"
     Comment "Key binding name in Settings, Controls (LF_WelcomeGuide)."
    }
    CustomStringTableItem "{BE1FAB450ACD3FB1}" {
     Id "LF-WelcomeGuide_Next"
     Target_en_us "NEXT"
     Comment "Welcome guide: button to the next page."
    }
    CustomStringTableItem "{D6F6871887DCCEC0}" {
     Id "LF-WelcomeGuide_Page"
     Target_en_us "Page %1 of %2"
     Comment "Welcome guide: page counter under the title; %1 is this page, %2 the number of pages."
    }
    CustomStringTableItem "{5393776CBA6F0043}" {
     Id "LF-WelcomeGuide_Prev"
     Target_en_us "PREVIOUS"
     Comment "Welcome guide: button to the page before."
    }
    CustomStringTableItem "{F15FD8A39C7470A1}" {
     Id "LF-WelcomeGuide_Title"
     Target_en_us "WELCOME"
     Comment "Welcome guide (new players): printed title of the sheet and the title of its toasts."
    }
    CustomStringTableItem "{5729CB5D596F7564}" {
     Id "LF-WelcomeGuide_Waypoint"
     Target_en_us "Set waypoint to the clerk"
     Comment "Welcome guide page 1 button: puts the current arrival errand (the clerk, for a new character) on the map and screen."
    }
    CustomStringTableItem "{B63C6D3F62307294}" {
     Id "LF-WelcomeGuide_WaypointNoStep"
     Target_en_us "Your arrival errands are done. Open the map (M) to set your own waypoint."
     Comment "Toast after Set waypoint when the arrival chain is finished."
    }
    CustomStringTableItem "{554234D75E7B1A34}" {
     Id "LF-WelcomeGuide_WaypointRefused"
     Target_en_us "No waypoint now. Try again in a moment."
     Comment "Toast after Set waypoint when the server could not set it (no body, rate limit)."
    }
    CustomStringTableItem "{773A3E90266AFD18}" {
     Id "LF-WelcomeGuide_WaypointSet"
     Target_en_us "Waypoint set: %1."
     Comment "Toast after Set waypoint; %1 is the name of the errand's place (the clerk, for a new character)."
    }
    CustomStringTableItem "{C37C0A98B9E81B46}" {
     Id "LF-Witness_AlreadyGiven"
     Target_en_us "You already told them that."
     Comment "Toast: the witness already gave this detail."
    }
    CustomStringTableItem "{D81009B11AFDDB8F}" {
     Id "LF-Witness_Anonymous"
     Target_en_us "an anonymous caller"
     Comment "Wanted board: the witness is not named."
    }
    CustomStringTableItem "{CDE539EBECEAEB35}" {
     Id "LF-Witness_Ask"
     Target_en_us "Ask what you saw"
     Comment "Interaction menu entry for on-duty police facing a witness."
    }
    CustomStringTableItem "{B5BA083D6C857087}" {
     Id "LF-Witness_BoardLine"
     Target_en_us "WITNESS (%1) on %2: %3"
     Comment "Wanted board witness line. %1 = who, %2 = offence, %3 = what they saw."
    }
    CustomStringTableItem "{D188FF1C27982766}" {
     Id "LF-Witness_BoardPlate"
     Target_en_us "Plate starts %1."
     Comment "Wanted board: the plate fragment a witness read. %1 = the characters."
    }
    CustomStringTableItem "{A5C8F0F7C66DA883}" {
     Id "LF-Witness_ForgotSome"
     Target_en_us "%1 cannot recall anything useful."
     Comment "Toast to the officer: a named witness saw the crime but remembered nothing describable. %1 = the witness name."
    }
    CustomStringTableItem "{9FF5E70D1F95C286}" {
     Id "LF-Witness_GaveStatement"
     Target_en_us "%1 gave a statement. It is on the wanted board."
     Comment "Toast to the officer: a named witness gave a statement. %1 = the witness name."
    }
    CustomStringTableItem "{1D0B9ADCB47A7284}" {
     Id "LF-Witness_NotSeen"
     Target_en_us "You didn't see that clearly enough."
     Comment "Toast: the witness stood too far away to make out this detail (or there was nothing to see)."
    }
    CustomStringTableItem "{A07851BD4B020CFC}" {
     Id "LF-Witness_Noted"
     Target_en_us "Noted. It goes on the {police} board."
     Comment "Toast: the witness statement was taken down."
    }
    CustomStringTableItem "{96AA484FA559A1BA}" {
     Id "LF-Witness_NothingSeen"
     Target_en_us "You didn't see anything."
     Comment "Toast: the witness saw no recent crime."
    }
    CustomStringTableItem "{B52F0B4191F415B2}" {
     Id "LF-Witness_Npc"
     Target_en_us "a passer-by"
     Comment "Wanted board: the witness is a town NPC."
    }
    CustomStringTableItem "{A27156349262AF6B}" {
     Id "LF-Witness_Statement"
     Target_en_us "Statement taken. It is on the wanted board."
     Comment "Toast to the officer: the witness gave a statement."
    }
    CustomStringTableItem "{46C33D356FF6A97D}" {
     Id "LF-Witness_Title"
     Target_en_us "Witness"
     Comment "Toast title for witness statements."
    }
    CustomStringTableItem "{63E93D8FD582854D}" {
     Id "LF-Witness_Unsure1"
     Target_en_us "%1: Hey, I am not sure."
     Comment "Toast: an NPC witness who saw nothing answers, noncommittal. %1 = the NPC name."
    }
    CustomStringTableItem "{C388BAAC69AA6FFA}" {
     Id "LF-Witness_Unsure2"
     Target_en_us "%1: I do not think I saw anything."
     Comment "Toast: noncommittal NPC answer, variant 2. %1 = the NPC name."
    }
    CustomStringTableItem "{FD870C5E92EE6005}" {
     Id "LF-Witness_Unsure3"
     Target_en_us "%1: Sorry, I could not say either way."
     Comment "Toast: noncommittal NPC answer, variant 3. %1 = the NPC name."
    }
    CustomStringTableItem "{52C8142385D923FE}" {
     Id "LF-Witness_Unsure4"
     Target_en_us "%1: I was not really looking, officer."
     Comment "Toast: noncommittal NPC answer, variant 4. %1 = the NPC name."
    }
    CustomStringTableItem "{A4A046A831C69FE0}" {
     Id "LF-Witness_Vague"
     Target_en_us "They can't recall anything useful."
     Comment "Toast to the officer: the witness remembered nothing they could describe."
    }
 }
}
