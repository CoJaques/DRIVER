.. _laboratoire6:

###########################################################
Laboratoire 6 --- Développement de drivers kernel-space III
###########################################################

.. only:: html

   .. figure:: ../images/logo_drv.png
      :width: 6cm
      :align: right

.. only:: latex

   .. figure:: ../images/banner_drv.png
      :width: 16cm
      :align: center


.. raw:: latex

   \newpage

=========
Objectifs
=========

* Faire du traitement différé des interruptions
* Utiliser les workqueues pour faire des actions "longues durées"
* Voir le mécanisme des listes chainées offert par le kernel
* Voir des méthodes de gestion du temps

===================
Matériel nécessaire
===================

Ce laboratoire utilise le même environnement que le laboratoire précédent,
qui peut donc être réutilisé.

=========================
Gestion des interruptions
=========================

Le mécanisme de gestion des interruptions utilisé jusqu'à présent peut s'avérer
inefficace lorsqu'on a des tâches conséquentes à réaliser pour traiter une interruption.
Par exemple, si on doit traiter une grande quantité de données suite à la pression d'un
bouton, il est évident qu'on ne peut pas effectuer ces opérations dans la routine d'interruption.
Il ne faut pas oublier non plus qu'il est **interdit** d'appeler dans une routine d'interruption des fonctions
qui pourraient s'endormir.

Vous avez vu pendant le cours plusieurs options pour repousser du travail à
l'extérieur du code de la routine d'interruption --- en particulier la distinction entre
*top half* (ou *hard irq*) et *bottom half*.
Parmi ces approches, les threaded irq handlers et les workqueues sont normalement
**la** façon de gérer les interruptions, sauf dans de cas très spécifiques.
Dans le tableau ci-dessous, les différentes alternatives conseillées sont listées

+--------------------------------+-------------------------------------+
| Durée IRQ handler              | Mécanisme conséillé                 |
+================================+=====================================+
| t <= 10 us                     | juste le hard irq                   |
+--------------------------------+-------------------------------------+
| 10 us < t < 100 us             | threaded irq handlers / workqueues  |
+--------------------------------+-------------------------------------+
| t >= 100 us, non-critical code | threaded irq handlers / workqueues  |
+--------------------------------+-------------------------------------+
| t >= 100 us, critical code     | tasklets                            |
+--------------------------------+-------------------------------------+

========
Exercice
========

On souhaite utiliser la carte la gestion du déroulement de match de tennis.

Pour rappels/informations, les règles du tennis sont les suivantes (simplifiées pour l'exercice) :

- Un match de tennis est composé de plusieurs *sets*.

  - En fonction du format, il faut gagner deux ou trois *sets* pour gagner le match.

- Un *set* est composé de plusieurs *jeux*.

  - Il faut gagner six *jeux* pour gagner un *set*.
  - Pour l'exercice, les règles des deux jeux d'écart et du *tie-break* ne seront pas prise en compte.

- Un *jeu* est composé de plusieurs *points*.

  - Pour gagner un *jeu*, il faut gagner au moins quatre *points* et avoir deux *points* d'écart avec l'adversaire
  - Pour des raisons historiques, le score suit le *paterne* "0 - 15 - 30 - 40" et on parle d'*avantage* (*Ad*) au joueur
    ayant gagné un point lors d'une égalité 40-40.

- Le joueur qui sert la balle est le même durant toute la durée d'un *jeu* et change à chaque *jeu*.
- Les joueurs changent de côté du terrain tous les deux jeux.

Exemple de déroulement :

.. code-block:: md

  Début du match

  J1 sert
  J1 gagne le point
  Score (jeu) : 15-0

  J1 gagne le point
  Score (jeu) : 30-0

  J2 gagne le point
  Score (jeu) : 30-15

  J1 gagne le point
  Score (jeu) : 40-15

  J1 gagne le point et le jeu
  Score (set) : 1-0


  J2 sert
  ...
  Score (jeu) : 40-40

  J2 gagne le point
  Score (jeu) : -Ad

  J1 gagne le point
  Score (jeu) : 40-40

  J1 gagne le point
  Score (jeu) : Ad-

  J1 gagne le point et le jeu
  Score (set) : 2-0

  Changement de côté
  J1 sert
  ...
  J2 gagne le jeu
  Score (set) : 2-1

  ...

  Score (set) : 5-4
  J2 sert
  ...
  J1 gagne le jeu et le set
  Score (match) : 1-0

  Nouveau set

  ...

  Score (set) : 5-3

  J2 sert
  ...
  J1 gagne le jeu, le set et le match

Fonctionnement de base
**********************

Implémentez un driver permettant d'utiliser la carte pour gérer les matchs selon les spécifications suivantes.

Par défaut, le driver est en attente d'un nouveau match et tout est éteint (7-segment et leds).
L'utilisateur doit alors appuyer sur :c:`KEY0` pour démarrer un match.

L'affichage suivant sera utilisé durant un match :

- Les affichages 7-segments :c:`HEX1-0` et :c:`HEX5-4` affichent le score du *jeu* actuel

  - En cas d'avantage (après égalité 40-40), :c:`Ad` est affiché pour le joueur ayant l'avantage et :c:`-` est affiché pour l'autre

- Les affichages 7-segment :c:`HEX2` et :c:`HEX3` affichent le score du *set* actuel
- Les leds :c:`LED0-2` et :c:`LED7-9` affichent le nombre de *set* gagné par les joueurs (le premier *set* gagné allume respectivement la LED 0 ou 9)
- Les leds :c:`LED4-5` indiqueront qui est le serveur actuel (commence par le joueur de droite)

Au début d'un match, le score de départ (0-0) s'affiche et il est ensuite possible d'indiquer les points gagnés à l'aide des boutons :c:`KEY0` et :c:`KEY3`.
La gestion des scores se fait alors automatiquement, sans autre entrée utilisateur (= détection automatique des *jeux/sets/match* gagnés).

Par défaut, le format est en mode deux *sets* gagnants, cette configuration se fera via un fichier *sysfs*.

Lorsque le match est terminé, le score du premier *set* s'affiche sur :c:`HEX4-5`, le deuxième sur :c:`HEX3-2` et le troisième sur :c:`HEX1-0`.
S'il y a plus de *set* (lors de partie en trois *sets* gagnant), l'affichage alterne toutes les trois secondes entre afficher les *sets* un à trois et les *sets* quatre et cinq.
Il faut alors appuyer sur :c:`KEY0` pour remettre le driver en mode par défaut (tout éteint).

Durant un match plusieurs annonces doivent également être faites :

- Lorsqu'un *jeu* est gagné :

  - :c:`G` (*Game*)
  - :c:`GS` (*Game and set*) : S'il s'agissait du dernier pour gagner le *set*
  - :c:`GSM` (*Game, set and match*) : S'il s'agissait du dernier pour gagne le *match*

- Lorsqu'un joueur peut gagner le *jeu* sur le prochain point :

  - :c:`GP` (*Game point*)
  - :c:`SP` (*Set point*) : Si le joueur peut gagner le *set*
  - :c:`MP` (*Match point*) : Si le joueur peut gagner le *match*

- :c:`SC` (*Side change*) : Lors d'un changement de côté

Ces annonces doivent être faites sur les 7-segments (lettre au plus proche) aux moments voulus (l'affichage du score est temporairement *caché*). Elles doivent être affichées pendant cinq secondes.
Si plusieurs annonces doivent s'afficher en même temps, elles devront être affichées
les unes après les autres. Les actions doivent rester possibles lors de l'affichage d'une annonce.

Historique et *char device*
***************************

En plus de ça, on souhaite conserver un historique des parties qui se sont déroulées.
Cet historique doit permettre de récupérer les scores de tous les matchs qui ont été joués (scores des différents *sets*)
et le nombre total de points jouer durant le match.
Si un match est en cours, celui-ci n'est pas inclus dans l'historique et le sera dès que le match est terminé.

Un *char device* doit être mis en place pour permettre la lecture de cet historique.

L'écriture doit permettre de supprimer une entrée à un indice donné.

Le format exact des données, que se soit pour la lecture ou l'écriture, est laissé libre, mais doit être en binaire (= pas d'ASCII).

Implémentez une application simple permettant d'interagir avec le *char device*. (Simple affichage des tous les scores et suppression d'un de ceux-ci)

SysFS
*****

Il doit également être possible d'accéder aux informations suivantes via des fichiers *sysfs* :

- Si un match est en cours
- Le score du match en cours (le score des différents *sets*)
- Le nombre total de match dans l'historique
- Le format (deux ou trois *sets* gagnant)

Le format doit être modifiable. Si un match est en cours, la modification sera acceptée si elle ne rend pas incohérente les scores actuels. (P.ex. passage du format 3 *set* gagnants à 2 alors que le score est de 2-2)

Lorsque aucun match n'est en cours, il doit être possible de démarrer un match avec un score prédéfini en écrivant dans le fichier permettant de récupérer le score du match courant.
L'écriture sera refusée si un match est en cours ou que les données reçues sont incohérentes (p. ex. deux *sets* sont fournis, mais le premier des deux n'est pas fini).

Contraintes
***********

Les contraintes suivantes devront être respectée :

- L'historique des matchs doit utiliser une liste chainée (:c:`list_head`) de Linux (`exemples d'utilisation <https://www.cs.wm.edu/~smherwig/courses/csci415-common/list/index.html>`__
  (attention, les exemples sont pour le userspace, mais basé sur l'implémentation du kernel), `api complète <https://www.kernel.org/doc/html/v6.1/core-api/kernel-api.html#list-management-functions>`__).
- La routine d'interruption principale pour les boutons ne doit que lire le bouton appuyé et réinitialisé le registre d'interruption. Une *threaded irq* devra être utilisée
  pour effectuer la logique nécessaire liée au bouton. (`exemple d'utilisation <https://github.com/ANSANJAY/KernelThreadedIRQ/blob/main/4_example/hello.c>`__)
- L'affichage des annonces doit utiliser une *workqueue*. Un nouveau *work* doit être créé pour chaque annonce (lancer par la *threaded irq*).
  (`petite documentation <https://linux-kernel-labs.github.io/refs/heads/master/labs/deferred_work.html#workqueues>`__, `documentation complète du fonctionnement et de l'api <https://www.kernel.org/doc/html/v6.1/core-api/workqueue.html>`__)
- Tous les accès concurrents potentiels aux variables/ressources doivent être protégés correctement.

.. note::

  Il est possible d'utiliser la fonction :c:`msleep()` dans :file:`linux/delay.h` pour attendre un temps donner dans le kernel.


=========================================
Travail à rendre et critères d'évaluation
=========================================

.. include:: ../consigne_rendu.rst.inc
