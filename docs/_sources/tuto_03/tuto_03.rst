.. _tutoriel3:

##############################################
Tutoriel 3 ---  REDS-adder driver v2.1 et v3.1
##############################################

.. only:: html

   .. figure:: ../images/logo_drv.png
      :width: 6cm
      :align: right

.. only:: latex

   .. figure:: ../images/banner_drv.png
      :width: 16cm
      :align: center

Dans ce tutoriel, on explorera davantage de possibilités offertes par le Linux
Device Model pour améliorer notre driver.

Les fichiers de ce tutoriel se trouvent dans le dépôt git.

**sysfs** nous permet d'exposer à l'utilisateur une série de propriétés, afin
de pouvoir aisément configurer et interagir avec notre dispositif.
Dans le contexte de notre driver, on utilisera sysfs pour :

* voir la taille maximale de la chaîne de caractères à chiffrer;
* voir et modifier le seuil utilisé pour le chiffrement;
* voir et modifier l'opération effectuée par le dispositif (chiffrement ou
  déchiffrement).

Le driver v2.1 est identique à la version 1.1, sauf qu'une entrée dans sysfs a
été ajoutée --- n'hésitez pas à comparer les deux versions du driver à l'aide
d'un outil tel que `meld <https://meldmerge.org/>`__, cela vous montrera comment
vous pouvez ajouter des fichiers dans sysfs.

Pouvoir changer ces propriétés à la volée introduit des problèmes potentiels : par
exemple, qu'est-ce qui va se passer si l'on modifie l'opération effectuée pendant
qu'elle est en cours ?
Pour éviter ces situations, une primitive de synchronisation (mutex) a été
introduite.

De plus, afin de mieux pouvoir gérer les données stockées dans notre driver, le
vecteur qu'on avait utilisé dans le tutoriel 2 a été remplacé par une KFIFO.

Le driver v3.1 est donc une version bien plus complète du driver pour notre
dispositif.
Essayez de comprendre ses mécanismes de fonctionnement et cherchez à
l'améliorer en rajoutant des fonctionnalités et en optimisant ses opérations.

*Have fun* et, bien sûr, on est à votre disposition pour toute question éventuelle ! :-)
