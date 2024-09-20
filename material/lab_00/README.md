# Exercice 1
Le problème est lié au fait qu'il y a des inclusions circulaire entre first et second, l'utilisation
des guars permet de régler le soucis.

# Exercice 2
Le résultat imprime les tailles de différents types.
Les tailles de str_array et de str_out sont différentes car str_array lorsqu'il est passé en paramètre de la fonction est converti en un pointeur sur le premier élément du tableau. 

De ce fait, la taille de str_array est la taille d'un pointeur, alors que la taille de str_out est la complète de la string. Le lien est donc évident avec le warning qui spécifie que sizeof sur un tableau passé en paramètre d'une fonction retourne la taille d'un char*.

Pour connaître la taille du tableau my_array, il faut diviser la taille du tableau complet par la taille d'un élément du tableau, pour cela il faut connaître le nobre d'éléments du tableau qui doit donc être passé en paramètre de la fonction. Pour un tableau statique, il suffit de faire sizeof(my_array).

Lors de la compilation en 32 bits la principale différence et la taille des pointeurs qui est de 4 octets, alors qu'en 64 bits elle est de 8 octets.

# Exercice 3
Mon système en little-endian, cf le code. De ce fait j'imprime les valeurs des octets dans l'ordre inverse de leur stockage en mémoire afin de rendre la lecture agréable.

# Exercice 4
car l'abstraction `#define Shape_MOVETO(obj, newx, newy)` est faite pour un plan 2d. Donc l'ajout de la 3e dimension nécessite de modifier la macro pour prendre en compte la nouvelle dimension et de modifier toutes les autres shapes.

# Exercice 6
en utilisant cette commande : `git grep -n '__attribute__' include/`

J'ai pu trouver les attributs suivants : 

| Attribut                                | Fonctionnalité                                                                                   |
|-----------------------------------------|-------------------------------------------------------------------------------------------------|
| `__attribute__((__format__(__printf__, c, c+1)))` | Indique que la fonction utilise le formatage printf, ce qui permet de vérifier les arguments.  |
| `__attribute__((unused))`               | Signale que la variable ou la fonction peut ne pas être utilisée, évitant ainsi les avertissements. |
| `__attribute__((__fallthrough__))`     | Indique intentionnellement qu'un `case` dans un `switch` tombe à travers sans `break`.        |
| `__attribute__((aligned(4)))`          | Aligne la variable sur une frontière de 4 bytes, ce qui peut améliorer les performances d'accès mémoire. |
| `__attribute__((noreturn))`             | Indique que la fonction ne retourne jamais, ce qui aide à l'optimisation et à la vérification du code. |
| `__attribute__((packed))`               | Force le compilateur à ne pas ajouter de remplissage entre les membres de la structure.        |
| `__attribute__((nonnull))`              | Indique que les pointeurs passés à la fonction ne doivent pas être nuls.                      |
| `__attribute__((__aligned__(SMP_CACHE_BYTES)))` | Aligne la variable sur la taille de cache du processeur pour optimiser l'accès.              |
