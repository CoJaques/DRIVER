#### Colin Jaques - Labo 1 DRV

# Exercice 1
la commande md permet de lire une valeur en mémoire, le b,w,l permet de définir le type d'affichage, b pour byte, w pour word et l pour long. le dernier paramètre permet de définir le nombre d'élément à lire.


La valeur des switche peut être lu à l'aide de la commande : ``` md.w 0xFF200040 0x1```

La valeur des leds peut être écrite à l'aide de la commande : ``` mw.w 0xFF200000 0x7f```

Ces adresses ont été trouvées à la page 50 du manuel de référence de la carte DE1-SoC.

Les accès non alignés cause des Data abort et cause un redémarrage de la carte. Il semblerait que les architectures ARM parfois empêche les accès non alignés pour des raisons de protection de la mémoire.

# Exercice 2

Voici le script réalisé : 
```assembly
setenv toggle_hex 'mw.l 0xFF200020 0xFF000000;
mw.l 0xFF200030 0x0000FFFF;
sleep 1;
mw.l 0xFF200020 0x00FFFFFF;
mw.l 0xFF200030 0x00000000;
sleep 1;
run toggle_hex'
```

# Exercice 3
Se référer au fichier ex3.c