-- Dentro do callback applyTheme, depois de addTextureToFont:
-- setFontCharMetrics(nomeDaFonte, caracteresUTF8, altura, deslocamentoVertical)
-- Altura > 0 multiplica a altura da celula; nao altera largura nem espacamento.
-- Deslocamento em unidades do tamanho do texto: positivo sobe, negativo desce.
-- A referencia e a base da celula (posY1), incluindo margens transparentes.
-- Os valores abaixo sao exemplos para calibracao visual, nao medidas de FONT.tga.
-- FONT.tga ja desenha g/p/y abaixo do b: estes deslocamentos sao adicionais.
assert(setFontCharMetrics("default", "b", 1.0, 0.0))
assert(setFontCharMetrics("default", "gp", 1.0, -0.10))
assert(setFontCharMetrics("default", "y", 1.0, -0.05))
assert(setFontCharMetrics("default", "a", 0.95, 0.0))
assert(setFontCharMetrics("default", "ÁÉÍÓÚ", 1.10, 0.0))

-- Para restaurar um grupo:
-- assert(setFontCharMetrics("default", "bgpyaÁÉÍÓÚ", 1.0, 0.0))
-- Registrar a textura novamente tambem restaura as metricas desses caracteres.
-- Retorna false sem alterar nenhum caractere se a fonte/caractere nao existir,
-- se o UTF-8 for invalido, ou se altura/deslocamento forem invalidos.
-- Funciona em texto 2D direto, 2D com buffer e 3D (no eixo vertical do texto).
