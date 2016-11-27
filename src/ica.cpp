/**
 * @file ica.c
 *
 * Implementation of ICA (Hyvarinen, 1999).
 */
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "database.h"
#include "timing.h"

matrix_t * fpica (matrix_t *X, matrix_t *whiteningMatrix, int max_iterations, precision_t epsilon);

/**
 * Compute the whitening matrix for a matrix X.
 *
 * The whitening (sphering) matrix, when applied to X, normalizes
 * the data to have zero mean and unity variance.
 *
 * @param X  input matrix
 * @param E  matrix of eigenvectors in columns
 * @param D  diagonal matrix of eigenvalues
 * @return whitening matrix
 */
matrix_t * m_whiten (matrix_t *X, matrix_t *E, matrix_t *D)
{
    // compute whitening matrix W_z = inv(sqrt(D)) * E'
    matrix_t *D_temp1 = m_copy(D);
    m_elem_apply(D_temp1, sqrtf);

    matrix_t *D_temp2 = m_inverse(D_temp1);
    matrix_t *whiteningMatrix = m_product(D_temp2, E, false, true);

    // cleanup
    m_free(D_temp1);
    m_free(D_temp2);

    return whiteningMatrix;
}

/**
 * Compute the independent components of a matrix X, which
 * consists of observations in columns.
 *
 * NOTE: Curently, X is transposed before it is processed, which
 * causes there to be two extra transposes, an extra mean subtraction,
 * and an extra PCA calculation. We should try to refactor ICA to use
 * X in its original form to eliminate these redundancies.
 *
 * @param X
 * @param max_iterations
 * @param epsilon
 * @return independent components of X in columns
 */
matrix_t * ICA (matrix_t *X, int max_iterations, precision_t epsilon)
{
    timing_push("  ICA");

    timing_push("    subtract mean from input matrix");

    // compute mixedsig = X', subtract mean column
    matrix_t *mixedsig = m_transpose(X);
    matrix_t *mixedmean = m_mean_column(mixedsig);

    m_subtract_columns(mixedsig, mixedmean);

    timing_pop();

    timing_push("    compute principal components of input matrix");

    // compute principal components
    matrix_t *D;
    matrix_t *E = PCA_rows(X, &D);

    timing_pop();

    timing_push("    compute whitening matrix and whitened input matrix");

    // compute whitened input
    matrix_t *whiteningMatrix = m_whiten(mixedsig, E, D);
    matrix_t *whitesig = m_product(whiteningMatrix, mixedsig);

    timing_pop();

    timing_push("    compute mixing matrix W");

    // compute mixing matrix
    matrix_t *W = fpica(whitesig, whiteningMatrix, max_iterations, epsilon);

    timing_pop();

    timing_push("    compute independent components");

    // compute independent components
    // icasig = W * mixedsig + (W * mixedmean) * ones(1, mixedsig->cols)
    matrix_t *icasig = m_product(W, mixedsig);
    matrix_t *icasig_temp1 = m_product(W, mixedmean);
    matrix_t *icasig_temp2 = m_ones(1, mixedsig->cols);
    matrix_t *icasig_temp3 = m_product(icasig_temp1, icasig_temp2);

    m_add(icasig, icasig_temp3);

    // compute W_ica = icasig'
    matrix_t *W_ica = m_transpose(icasig);

    timing_pop();

    timing_pop();

    // cleanup
    m_free(mixedsig);
    m_free(mixedmean);
    m_free(E);
    m_free(D);
    m_free(whiteningMatrix);
    m_free(whitesig);
    m_free(W);
    m_free(icasig_temp1);
    m_free(icasig_temp2);
    m_free(icasig_temp3);
    m_free(icasig);

    return W_ica;
}

/**
 * Compute the third power (cube) of a number.
 *
 * @param x
 * @return x ^ 3
 */
precision_t pow3(precision_t x)
{
    return pow(x, 3);
}

/**
 * Compute the mixing matrix W for an input matrix X using the deflation
 * approach and the nonlinearity functions pow3. The input matrix should
 * already be whitened.
 *
 * @param X
 * @param whiteningMatrix
 * @param max_iterations
 * @param epsilon
 * @return mixing matrix W
 */
matrix_t * fpica (matrix_t *X, matrix_t *whiteningMatrix, int max_iterations, precision_t epsilon)
{
    int vectorSize = X->rows;
    int numSamples = X->cols;

    matrix_t *B = m_zeros(vectorSize, vectorSize);
    matrix_t *W = m_zeros(vectorSize, whiteningMatrix->cols);

    int i;
    for ( i = 0; i < vectorSize; i++ ) {
        if ( VERBOSE ) {
            printf("round %d\n", i + 1);
        }

        // initialize w as a Gaussian random vector
        matrix_t *w = m_random(vectorSize, 1);

        // compute w = (w - B * B' * w), normalize w
        matrix_t *w_temp1 = m_product(B, B, false, true);
        matrix_t *w_temp2 = m_product(w_temp1, w);

        m_subtract(w, w_temp2);
        m_elem_mult(w, 1 / m_norm(w));

        m_free(w_temp1);
        m_free(w_temp2);

        // initialize w0
        matrix_t *w0 = m_zeros(w->rows, w->cols);

        int j;
        for ( j = 0; j < max_iterations; j++ ) {
            // compute w = (w - B * B' * w), normalize w
            w_temp1 = m_product(B, B, false, true);
            w_temp2 = m_product(w_temp1, w);

            m_subtract(w, w_temp2);
            m_elem_mult(w, 1 / m_norm(w));

            m_free(w_temp1);
            m_free(w_temp2);

            // compute w_delta1 = w - w0
            matrix_t *w_delta1 = m_copy(w);
            m_subtract(w_delta1, w0);

            // compute w_delta2 = w + w0
            matrix_t *w_delta2 = m_copy(w);
            m_add(w_delta2, w0);

            // determine whether the direction of w and w0 are equal
            precision_t norm1 = m_norm(w_delta1);
            precision_t norm2 = m_norm(w_delta2);

            // printf("%lf %lf\n", norm1, norm2);

            int converged = (norm1 < epsilon) || (norm2 < epsilon);

            m_free(w_delta1);
            m_free(w_delta2);

            // terminate round if w converges
            if ( converged ) {
                // save B(:, i) = w
                m_assign_column(B, i, w, 0);

                // save W(i, :) = w' * whiteningMatrix
                matrix_t *W_temp1 = m_product(w, whiteningMatrix, true, false);

                m_assign_row(W, i, W_temp1, 0);

                // cleanup
                m_free(W_temp1);

                // continue to the next round
                break;
            }

            // update w0
            m_assign_column(w0, 0, w, 0);

            // compute w = X * ((X' * w) .^ 3) / numSamples - 3 * w
            w_temp1 = m_product(X, w, true, false);
            m_elem_apply(w_temp1, pow3);

            w_temp2 = m_copy(w);
            m_elem_mult(w_temp2, 3);

            w = m_product(X, w_temp1);
            m_elem_mult(w, 1.0 / numSamples);
            m_subtract(w, w_temp2);

            // normalize w
            m_elem_mult(w, 1 / m_norm(w));

            m_free(w_temp1);
            m_free(w_temp2);
        }

        if ( VERBOSE ) {
            printf("iterations: %d\n", j);
        }

        m_free(w);
        m_free(w0);
    }

    m_free(B);

    return W;
}
