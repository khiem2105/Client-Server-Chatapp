# Simple chat app using C
- /nick new_nickname pour modifier nick name
- /msg destination message pour envoyer un message privé à la destination
- /alert message pour envoyer un message d'alert à tout le monde
- /alert destination message pour envoyer un message d'alert à la destination
- /file filename pour envoyer le fichier filenmae à tout le monde
- En plus, affichage l'heure de message de côté client, enregistrer l'histoire du message dans un fichier
/log/nickname.txt. Envoyer l'histoire au client si il reconnecte. (il faut créer un répertoire log)

- Démarrer: make -> ./server -> ./client (pluiseur fois pour démarrer plusieurs client)