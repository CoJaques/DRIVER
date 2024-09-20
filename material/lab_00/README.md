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