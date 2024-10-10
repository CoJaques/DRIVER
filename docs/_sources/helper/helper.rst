.. _helper:

################
Helper
################

.. only:: latex

   .. figure:: ../images/banner_drv.png
      :width: 16cm
      :align: center

=============================
Vérification du style de code
=============================

Afin de vérifier le respect du style de code du kernel Linux, le script :bash:`style_check` est mis à disposition.
Celui-ci se trouve dans le dossier :bash:`material/helper`. Il prend comme unique argument le fichier à contrôler.
Avant utilisation, il faut installer les outils :bash:`meld` et :bash:`clang-format` (:bash:`sudo apt install meld clang-format`).


L'outil :bash:`meld` est une interface graphique montrant les différences entre deux fichiers côte à côte et d'appliquer les changements directement dans le fichier voulu.
Il est très utile lors de merge ou pour voir ce qui à changer dans le code avec Git.
Les commandes :bash:`git difftool` et :bash:`git mergetool` dans un repo Git permet de l'utiliser pour ces cas-là.

Le script fourni fait en sorte que le fichier de gauche soit l'original et celui de droite la version formatée par :bash:`clang-format`.
Ce dernier est en fichier temporaire et sera supprimé après fermeture de l'outil. Vous pouvez cependant appliquer les différences sur le fichier original !
:bash:`clang-format` est un bon outil de formatage automatique, mais ne permet pas de tout faire, garder un oeil critique sur les changements proposés.

Pour simplifier son utilisation, un lien symbolique peut être créé vers le script dans le dossier :bash:`/usr/bin/local`

.. code-block:: console

    $ sudo ln -s $(pwd)/material/helper/style_check.sh /usr/local/bin/style_check

.. figure:: ./images/meld.png
      :align: center

      Exemple de meld sur un fichier, avec à gauche le fichier original et à droite les corrections proposées automatiquement.
      Ici, certaines indentations était faite avec des espaces et sont donc corrigées en tabulation, également certains espacements
      n'étaient pas correctement respecté.
